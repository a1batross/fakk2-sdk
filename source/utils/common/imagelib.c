// imagelib.c

#include "cmdlib.h"
#include "imagelib.h"

qboolean image_has_alpha;

int fgetLittleShort( FILE *f )
{
	byte b1, b2;

	b1 = fgetc( f );
	b2 = fgetc( f );

	return (short)( b1 + b2 * 256 );
}

int fgetLittleLong( FILE *f )
{
	byte b1, b2, b3, b4;

	b1 = fgetc( f );
	b2 = fgetc( f );
	b3 = fgetc( f );
	b4 = fgetc( f );

	return b1 + ( b2 << 8 ) + ( b3 << 16 ) + ( b4 << 24 );
}

/*
============================================================================

						LBM STUFF

============================================================================
*/


typedef unsigned char UBYTE;
// conflicts with windows typedef short			WORD;
typedef unsigned short UWORD;
typedef long LONG;

typedef enum
{
	ms_none,
	ms_mask,
	ms_transcolor,
	ms_lasso
} mask_t;

typedef enum
{
	cm_none,
	cm_rle1
} compress_t;

typedef struct
{
	UWORD w, h;
	short x, y;
	UBYTE nPlanes;
	UBYTE masking;
	UBYTE compression;
	UBYTE pad1;
	UWORD transparentColor;
	UBYTE xAspect, yAspect;
	short pageWidth, pageHeight;
} bmhd_t;

extern bmhd_t bmhd; // will be in native byte order



#define FORMID ( 'F' + ( 'O' << 8 ) + ((int)'R' << 16 ) + ((int)'M' << 24 ))
#define ILBMID ( 'I' + ( 'L' << 8 ) + ((int)'B' << 16 ) + ((int)'M' << 24 ))
#define PBMID  ( 'P' + ( 'B' << 8 ) + ((int)'M' << 16 ) + ((int)' ' << 24 ))
#define BMHDID ( 'B' + ( 'M' << 8 ) + ((int)'H' << 16 ) + ((int)'D' << 24 ))
#define BODYID ( 'B' + ( 'O' << 8 ) + ((int)'D' << 16 ) + ((int)'Y' << 24 ))
#define CMAPID ( 'C' + ( 'M' << 8 ) + ((int)'A' << 16 ) + ((int)'P' << 24 ))


bmhd_t bmhd;

int Align( int l )
{
	if( l & 1 )
		return l + 1;
	return l;
}

/*
================
LBMRLEdecompress

Source must be evenly aligned!
================
*/
byte *LBMRLEDecompress( byte *source, byte *unpacked, int bpwidth )
{
	int  count;
	byte b, rept;

	count = 0;

	do
	{
		rept = *source++;

		if( rept > 0x80 )
		{
			rept = ( rept ^ 0xff ) + 2;
			b = *source++;
			memset( unpacked, b, rept );
			unpacked += rept;
		}
		else if( rept < 0x80 )
		{
			rept++;
			memcpy( unpacked, source, rept );
			unpacked += rept;
			source += rept;
		}
		else
			rept = 0; // rept of 0x80 is NOP

		count += rept;

	}
	while( count < bpwidth );

	if( count > bpwidth )
		Error( "Decompression exceeded width!\n" );


	return source;
}

/*
=================
LoadLBM
=================
*/
void LoadLBM( char *filename, byte **picture, byte **palette )
{
	byte *LBMbuffer, *picbuffer, *cmapbuffer;
	int  y;
	byte *LBM_P, *LBMEND_P;
	byte *pic_p;
	byte *body_p;

	int  formtype, formlength;
	int  chunktype, chunklength;

// qiet compiler warnings
	picbuffer = NULL;
	cmapbuffer = NULL;

//
// load the LBM
//
	LoadFile( filename, (void **)&LBMbuffer );

//
// parse the LBM header
//
	LBM_P = LBMbuffer;
	if( *(int *)LBMbuffer != LittleLong( FORMID ))
		Error( "No FORM ID at start of file!\n" );

	LBM_P += 4;
	formlength = BigLong( *(int *)LBM_P );
	LBM_P += 4;
	LBMEND_P = LBM_P + Align( formlength );

	formtype = LittleLong( *(int *)LBM_P );

	if( formtype != ILBMID && formtype != PBMID )
		Error( "Unrecognized form type: %c%c%c%c\n", formtype & 0xff,
		       ( formtype >> 8 ) & 0xff, ( formtype >> 16 ) & 0xff, ( formtype >> 24 ) & 0xff );

	LBM_P += 4;

//
// parse chunks
//

	while( LBM_P < LBMEND_P )
	{
		chunktype = LBM_P[0] + ( LBM_P[1] << 8 ) + ( LBM_P[2] << 16 ) + ( LBM_P[3] << 24 );
		LBM_P += 4;
		chunklength = LBM_P[3] + ( LBM_P[2] << 8 ) + ( LBM_P[1] << 16 ) + ( LBM_P[0] << 24 );
		LBM_P += 4;

		switch( chunktype )
		{
		case BMHDID:
			memcpy( &bmhd, LBM_P, sizeof( bmhd ));
			bmhd.w = BigShort( bmhd.w );
			bmhd.h = BigShort( bmhd.h );
			bmhd.x = BigShort( bmhd.x );
			bmhd.y = BigShort( bmhd.y );
			bmhd.pageWidth = BigShort( bmhd.pageWidth );
			bmhd.pageHeight = BigShort( bmhd.pageHeight );
			break;

		case CMAPID:
			cmapbuffer = malloc( 768 );
			memset( cmapbuffer, 0, 768 );
			memcpy( cmapbuffer, LBM_P, chunklength );
			break;

		case BODYID:
			body_p = LBM_P;

			pic_p = picbuffer = malloc( bmhd.w * bmhd.h );
			if( formtype == PBMID )
			{
				//
				// unpack PBM
				//
				for( y = 0; y < bmhd.h; y++, pic_p += bmhd.w )
				{
					if( bmhd.compression == cm_rle1 )
						body_p = LBMRLEDecompress((byte *)body_p,
									  pic_p, bmhd.w );
					else if( bmhd.compression == cm_none )
					{
						memcpy( pic_p, body_p, bmhd.w );
						body_p += Align( bmhd.w );
					}
				}

			}
			else
			{
				//
				// unpack ILBM
				//
				Error( "%s is an interlaced LBM, not packed", filename );
			}
			break;
		}

		LBM_P += Align( chunklength );
	}

	free( LBMbuffer );

	*picture = picbuffer;

	if( palette )
		*palette = cmapbuffer;
}

/*
============================================================================

							WRITE LBM

============================================================================
*/

/*
==============
WriteLBMfile
==============
*/
void WriteLBMfile( char *filename, byte *data, int width, int height, byte *palette )
{
	byte   *lbm, *lbmptr;
	int    *formlength, *bmhdlength, *cmaplength, *bodylength;
	int    length;
	bmhd_t basebmhd;

	lbm = lbmptr = malloc( width * height + 1000 );

//
// start FORM
//
	*lbmptr++ = 'F';
	*lbmptr++ = 'O';
	*lbmptr++ = 'R';
	*lbmptr++ = 'M';

	formlength = (int *)lbmptr;
	lbmptr += 4; // leave space for length

	*lbmptr++ = 'P';
	*lbmptr++ = 'B';
	*lbmptr++ = 'M';
	*lbmptr++ = ' ';

//
// write BMHD
//
	*lbmptr++ = 'B';
	*lbmptr++ = 'M';
	*lbmptr++ = 'H';
	*lbmptr++ = 'D';

	bmhdlength = (int *)lbmptr;
	lbmptr += 4; // leave space for length

	memset( &basebmhd, 0, sizeof( basebmhd ));
	basebmhd.w = BigShort((short)width );
	basebmhd.h = BigShort((short)height );
	basebmhd.nPlanes = BigShort( 8 );
	basebmhd.xAspect = BigShort( 5 );
	basebmhd.yAspect = BigShort( 6 );
	basebmhd.pageWidth = BigShort((short)width );
	basebmhd.pageHeight = BigShort((short)height );

	memcpy( lbmptr, &basebmhd, sizeof( basebmhd ));
	lbmptr += sizeof( basebmhd );

	length = lbmptr - (byte *)bmhdlength - 4;
	*bmhdlength = BigLong( length );
	if( length & 1 )
		*lbmptr++ = 0; // pad chunk to even offset

//
// write CMAP
//
	*lbmptr++ = 'C';
	*lbmptr++ = 'M';
	*lbmptr++ = 'A';
	*lbmptr++ = 'P';

	cmaplength = (int *)lbmptr;
	lbmptr += 4; // leave space for length

	memcpy( lbmptr, palette, 768 );
	lbmptr += 768;

	length = lbmptr - (byte *)cmaplength - 4;
	*cmaplength = BigLong( length );
	if( length & 1 )
		*lbmptr++ = 0; // pad chunk to even offset

//
// write BODY
//
	*lbmptr++ = 'B';
	*lbmptr++ = 'O';
	*lbmptr++ = 'D';
	*lbmptr++ = 'Y';

	bodylength = (int *)lbmptr;
	lbmptr += 4; // leave space for length

	memcpy( lbmptr, data, width * height );
	lbmptr += width * height;

	length = lbmptr - (byte *)bodylength - 4;
	*bodylength = BigLong( length );
	if( length & 1 )
		*lbmptr++ = 0; // pad chunk to even offset

//
// done
//
	length = lbmptr - (byte *)formlength - 4;
	*formlength = BigLong( length );
	if( length & 1 )
		*lbmptr++ = 0; // pad chunk to even offset

//
// write output file
//
	SaveFile( filename, lbm, lbmptr - lbm );
	free( lbm );
}

/*
============================================================================

LOAD PCX

============================================================================
*/

typedef struct
{
	char           manufacturer;
	char           version;
	char           encoding;
	char           bits_per_pixel;
	unsigned short xmin, ymin, xmax, ymax;
	unsigned short hres, vres;
	unsigned char  palette[48];
	char           reserved;
	char           color_planes;
	unsigned short bytes_per_line;
	unsigned short palette_type;
	char           filler[58];
	unsigned char  data;                    // unbounded
} pcx_t;


/*
==============
LoadPCX
==============
*/
void LoadPCX( char *filename, byte **pic, byte **palette, int *width, int *height )
{
	byte  *raw;
	pcx_t *pcx;
	int   x, y;
	int   len;
	int   dataByte, runLength;
	byte  *out, *pix;

	//
	// load the file
	//
	len = LoadFile( filename, (void **)&raw );

	//
	// parse the PCX file
	//
	pcx = (pcx_t *)raw;
	raw = &pcx->data;

	pcx->xmin = LittleShort( pcx->xmin );
	pcx->ymin = LittleShort( pcx->ymin );
	pcx->xmax = LittleShort( pcx->xmax );
	pcx->ymax = LittleShort( pcx->ymax );
	pcx->hres = LittleShort( pcx->hres );
	pcx->vres = LittleShort( pcx->vres );
	pcx->bytes_per_line = LittleShort( pcx->bytes_per_line );
	pcx->palette_type = LittleShort( pcx->palette_type );

	if( pcx->manufacturer != 0x0a
	    || pcx->version != 5
	    || pcx->encoding != 1
	    || pcx->bits_per_pixel != 8
	    || pcx->xmax >= 640
	    || pcx->ymax >= 480 )
		Error( "Bad pcx file %s", filename );

	if( palette )
	{
		*palette = malloc( 768 );
		memcpy( *palette, (byte *)pcx + len - 768, 768 );
	}

	if( width )
		*width = pcx->xmax + 1;
	if( height )
		*height = pcx->ymax + 1;

	if( !pic )
		return;

	out = malloc(( pcx->ymax + 1 ) * ( pcx->xmax + 1 ));
	if( !out )
		Error( "Skin_Cache: couldn't allocate" );

	*pic = out;

	pix = out;

	for( y = 0; y <= pcx->ymax; y++, pix += pcx->xmax + 1 )
	{
		for( x = 0; x <= pcx->xmax; )
		{
			dataByte = *raw++;

			if(( dataByte & 0xC0 ) == 0xC0 )
			{
				runLength = dataByte & 0x3F;
				dataByte = *raw++;
			}
			else
				runLength = 1;

			while( runLength-- > 0 )
				pix[x++] = dataByte;
		}

	}

	if( raw - (byte *)pcx > len )
		Error( "PCX file %s was malformed", filename );

	free( pcx );
}

/*
==============
WritePCXfile
==============
*/
void WritePCXfile( char *filename, byte *data, int width, int height, byte *palette )
{
	int   i, j, length;
	pcx_t *pcx;
	byte  *pack;

	pcx = malloc( width * height * 2 + 1000 );
	memset( pcx, 0, sizeof( *pcx ));

	pcx->manufacturer = 0x0a; // PCX id
	pcx->version = 5;         // 256 color
	pcx->encoding = 1;        // uncompressed
	pcx->bits_per_pixel = 8;  // 256 color
	pcx->xmin = 0;
	pcx->ymin = 0;
	pcx->xmax = LittleShort((short)( width - 1 ));
	pcx->ymax = LittleShort((short)( height - 1 ));
	pcx->hres = LittleShort((short)width );
	pcx->vres = LittleShort((short)height );
	pcx->color_planes = 1; // chunky image
	pcx->bytes_per_line = LittleShort((short)width );
	pcx->palette_type = LittleShort( 1 ); // not a grey scale

	// pack the image
	pack = &pcx->data;

	for( i = 0; i < height; i++ )
	{
		for( j = 0; j < width; j++ )
		{
			if(( *data & 0xc0 ) != 0xc0 )
				*pack++ = *data++;
			else
			{
				*pack++ = 0xc1;
				*pack++ = *data++;
			}
		}
	}

	// write the palette
	*pack++ = 0x0c; // palette ID byte
	for( i = 0; i < 768; i++ )
		*pack++ = *palette++;

// write output file
	length = pack - (byte *)pcx;
	SaveFile( filename, pcx, length );

	free( pcx );
}

/*
============================================================================

LOAD BMP

============================================================================
*/


/*

// we can't just use these structures, because
// compiler structure alignment will not be portable
// on this unaligned stuff

typedef struct tagBITMAPFILEHEADER { // bmfh
        WORD    bfType;				// BM
        DWORD   bfSize;
        WORD    bfReserved1;
        WORD    bfReserved2;
        DWORD   bfOffBits;
} BITMAPFILEHEADER;

typedef struct tagBITMAPINFOHEADER{ // bmih
   DWORD  biSize;
   LONG   biWidth;
   LONG   biHeight;
   WORD   biPlanes;
   WORD   biBitCount
   DWORD  biCompression;
   DWORD  biSizeImage;
   LONG   biXPelsPerMeter;
   LONG   biYPelsPerMeter;
   DWORD  biClrUsed;
   DWORD  biClrImportant;
} BITMAPINFOHEADER;

typedef struct tagBITMAPINFO { // bmi
   BITMAPINFOHEADER bmiHeader;
   RGBQUAD          bmiColors[1];
} BITMAPINFO;

typedef struct tagBITMAPCOREHEADER { // bmch
        DWORD   bcSize;
        WORD    bcWidth;
        WORD    bcHeight;
        WORD    bcPlanes;
        WORD    bcBitCount;
} BITMAPCOREHEADER;

typedef struct _BITMAPCOREINFO {    // bmci
        BITMAPCOREHEADER  bmciHeader;
        RGBTRIPLE         bmciColors[1];
} BITMAPCOREINFO;

*/

/*
==============
LoadBMP
==============
*/
void LoadBMP( char *filename, byte **pic, byte **palette, int *width, int *height )
{
	byte     *out;
	FILE     *fin;
	int      i;
	int      bfSize;
	int      bfOffBits;
	int      structSize;
	int      bcWidth;
	int      bcHeight;
	int      bcPlanes;
	int      bcBitCount;
	byte     bcPalette[1024];
	qboolean flipped;

	fin = fopen( filename, "rb" );
	if( !fin )
	{
		Error( "Couldn't read %s", filename );
	}

	i = fgetLittleShort( fin );
	if( i != 'B' + ( 'M' << 8 ))
	{
		Error( "%s is not a bmp file", filename );
	}

	bfSize = fgetLittleLong( fin );
	fgetLittleShort( fin );
	fgetLittleShort( fin );
	bfOffBits = fgetLittleLong( fin );

	// the size will tell us if it is a
	// bitmapinfo or a bitmapcore
	structSize = fgetLittleLong( fin );
	if( structSize == 40 )
	{
		// bitmapinfo
		bcWidth = fgetLittleLong( fin );
		bcHeight = fgetLittleLong( fin );
		bcPlanes = fgetLittleShort( fin );
		bcBitCount = fgetLittleShort( fin );

		fseek( fin, 24, SEEK_CUR );

		if( palette )
		{
			fread( bcPalette, 1, 1024, fin );
			*palette = malloc( 768 );

			for( i = 0; i < 256; i++ )
			{
				( *palette )[i * 3 + 0] = bcPalette[i * 4 + 2];
				( *palette )[i * 3 + 1] = bcPalette[i * 4 + 1];
				( *palette )[i * 3 + 2] = bcPalette[i * 4 + 0];
			}
		}
	}
	else if( structSize == 12 )
	{
		// bitmapcore
		bcWidth = fgetLittleShort( fin );
		bcHeight = fgetLittleShort( fin );
		bcPlanes = fgetLittleShort( fin );
		bcBitCount = fgetLittleShort( fin );

		if( palette )
		{
			fread( bcPalette, 1, 768, fin );
			*palette = malloc( 768 );

			for( i = 0; i < 256; i++ )
			{
				( *palette )[i * 3 + 0] = bcPalette[i * 3 + 2];
				( *palette )[i * 3 + 1] = bcPalette[i * 3 + 1];
				( *palette )[i * 3 + 2] = bcPalette[i * 3 + 0];
			}
		}
	}
	else
	{
		Error( "%s had strange struct size", filename );
	}

	if( bcPlanes != 1 )
	{
		Error( "%s was not a single plane image", filename );
	}

	if( bcBitCount != 8 )
	{
		Error( "%s was not an 8 bit image", filename );
	}

	if( bcHeight < 0 )
	{
		bcHeight = -bcHeight;
		flipped = qtrue;
	}
	else
	{
		flipped = qfalse;
	}

	if( width )
		*width = bcWidth;
	if( height )
		*height = bcHeight;

	if( !pic )
	{
		fclose( fin );
		return;
	}

	out = malloc( bcWidth * bcHeight );
	*pic = out;
	fseek( fin, bfOffBits, SEEK_SET );

	if( flipped )
	{
		for( i = 0; i < bcHeight; i++ )
		{
			fread( out + bcWidth * ( bcHeight - 1 - i ), 1, bcWidth, fin );
		}
	}
	else
	{
		fread( out, 1, bcWidth * bcHeight, fin );
	}

	fclose( fin );
}

/*
============================================================================

LOAD IMAGE

============================================================================
*/

/*
==============
Load256Image

Will load either an lbm or pcx, depending on extension.
Any of the return pointers can be NULL if you don't want them.
==============
*/
void Load256Image( char *name, byte **pixels, byte **palette, int *width, int *height )
{
	char ext[128];

	ExtractFileExtension( name, ext );
	if( !Q_stricmp( ext, "lbm" ))
	{
		LoadLBM( name, pixels, palette );
		if( width )
			*width = bmhd.w;
		if( height )
			*height = bmhd.h;
	}
	else if( !Q_stricmp( ext, "pcx" ))
	{
		LoadPCX( name, pixels, palette, width, height );
	}
	else if( !Q_stricmp( ext, "bmp" ))
	{
		LoadBMP( name, pixels, palette, width, height );
	}
	else
		Error( "%s doesn't have a known image extension", name );
}

/*
==============
Save256Image

Will save either an lbm or pcx, depending on extension.
==============
*/
void Save256Image( char *name, byte *pixels, byte *palette, int width, int height )
{
	char ext[128];

	ExtractFileExtension( name, ext );
	if( !Q_stricmp( ext, "lbm" ))
	{
		WriteLBMfile( name, pixels, width, height, palette );
	}
	else if( !Q_stricmp( ext, "pcx" ))
	{
		WritePCXfile( name, pixels, width, height, palette );
	}
	else
		Error( "%s doesn't have a known image extension", name );
}

/*
============================================================================

TARGA IMAGE

============================================================================
*/

typedef struct _TargaHeader
{
	unsigned char  id_length, colormap_type, image_type;
	unsigned short colormap_index, colormap_length;
	unsigned char  colormap_size;
	unsigned short x_origin, y_origin, width, height;
	unsigned char  pixel_size, attributes;
} TargaHeader;

/*
=============
LoadTGA
=============
*/
void LoadTGA( char *name, byte **pixels, int *width, int *height )
{
	int         columns, rows, numPixels;
	byte        *pixbuf;
	int         row, column;
	FILE        *fin;
	byte        *targa_rgba;
	TargaHeader targa_header;

	*pixels = 0;
	fin = fopen( name, "rb" );
	if( !fin )
		Error( "Couldn't read %s", name );

	targa_header.id_length = fgetc( fin );
	targa_header.colormap_type = fgetc( fin );
	targa_header.image_type = fgetc( fin );

	targa_header.colormap_index = fgetLittleShort( fin );
	targa_header.colormap_length = fgetLittleShort( fin );
	targa_header.colormap_size = fgetc( fin );
	targa_header.x_origin = fgetLittleShort( fin );
	targa_header.y_origin = fgetLittleShort( fin );
	targa_header.width = fgetLittleShort( fin );
	targa_header.height = fgetLittleShort( fin );
	targa_header.pixel_size = fgetc( fin );
	targa_header.attributes = fgetc( fin );

	if( targa_header.image_type != 2
	    && targa_header.image_type != 10
	    && targa_header.image_type != 3 )
	{
		printf( "LoadTGA: Only type 2 (RGB), 3 (gray), and 10 (RGB) TGA images supported\n" );
		return;
	}

	if( targa_header.colormap_type != 0 )
		Error( "LoadTGA: colormaps not supported" );

	if( targa_header.image_type != 3
	    && targa_header.pixel_size != 32
	    && targa_header.pixel_size != 24 )
	{
		Error( "LoadTGA: unsupported pixel size" );
	}

	columns = targa_header.width;
	rows = targa_header.height;
	numPixels = columns * rows;

	if( width )
		*width = columns;
	if( height )
		*height = rows;
	targa_rgba = malloc( numPixels * 4 );
	if( pixels )
		*pixels = targa_rgba;
	else
		return;

	if( targa_header.id_length != 0 )
		fseek( fin, targa_header.id_length, SEEK_CUR ); // skip TARGA image comment

	if(
		( targa_header.image_type == 2 )
		|| ( targa_header.image_type == 3 )
		)
	{ // Uncompressed, RGB or grayscale images
		for( row = rows - 1; row >= 0; row-- )
		{
			pixbuf = targa_rgba + row * columns * 4;
			for( column = 0; column < columns; column++ )
			{
				unsigned char red, green, blue, alphabyte;
				switch( targa_header.pixel_size )
				{
				case 8:
					blue = getc( fin );
					green = blue;
					red = blue;
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = 255;
					break;

				case 24:

					blue = getc( fin );
					green = getc( fin );
					red = getc( fin );
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = 255;
					break;
				case 32:
					image_has_alpha = qtrue;
					blue = getc( fin );
					green = getc( fin );
					red = getc( fin );
					alphabyte = getc( fin );
					*pixbuf++ = red;
					*pixbuf++ = green;
					*pixbuf++ = blue;
					*pixbuf++ = alphabyte;
					break;
				}
			}
		}
	}
	else if( targa_header.image_type == 10 ) // Runlength encoded RGB images
	{
		unsigned char red, green, blue, alphabyte, packetHeader, packetSize, j;
		for( row = rows - 1; row >= 0; row-- )
		{
			pixbuf = targa_rgba + row * columns * 4;
			for( column = 0; column < columns; )
			{
				packetHeader = getc( fin );
				packetSize = 1 + ( packetHeader & 0x7f );
				if( packetHeader & 0x80 ) // run-length packet
				{
					switch( targa_header.pixel_size )
					{
					case 24:
						blue = getc( fin );
						green = getc( fin );
						red = getc( fin );
						alphabyte = 255;
						break;
					case 32:
						image_has_alpha = qtrue;
						blue = getc( fin );
						green = getc( fin );
						red = getc( fin );
						alphabyte = getc( fin );
						break;
					}

					for( j = 0; j < packetSize; j++ )
					{
						image_has_alpha = qtrue;
						*pixbuf++ = red;
						*pixbuf++ = green;
						*pixbuf++ = blue;
						*pixbuf++ = alphabyte;
						column++;
						if( column == columns ) // run spans across rows
						{
							column = 0;
							if( row > 0 )
								row--;
							else
								goto breakOut;
							pixbuf = targa_rgba + row * columns * 4;
						}
					}
				}
				else // non run-length packet
				{
					for( j = 0; j < packetSize; j++ )
					{
						switch( targa_header.pixel_size )
						{
						case 24:
							blue = getc( fin );
							green = getc( fin );
							red = getc( fin );
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = 255;
							break;
						case 32:
							image_has_alpha = qtrue;
							blue = getc( fin );
							green = getc( fin );
							red = getc( fin );
							alphabyte = getc( fin );
							*pixbuf++ = red;
							*pixbuf++ = green;
							*pixbuf++ = blue;
							*pixbuf++ = alphabyte;
							break;
						}
						column++;
						if( column == columns ) // pixel packet run spans across rows
						{
							column = 0;
							if( row > 0 )
								row--;
							else
								goto breakOut;
							pixbuf = targa_rgba + row * columns * 4;
						}
					}
				}
			}
breakOut:               ;
		}
	}

	fclose( fin );
}

/*
================
WriteTGA
================
*/
void WriteTGA( char *filename, byte *data, int width, int height )
{
	byte *buffer;
	int  i;
	int  c;
	FILE *f;

	buffer = malloc( width * height * 4 + 18 );
	memset( buffer, 0, 18 );
	buffer[2] = 2; // uncompressed type
	buffer[12] = width & 255;
	buffer[13] = width >> 8;
	buffer[14] = height & 255;
	buffer[15] = height >> 8;
	buffer[16] = 32; // pixel size

	// swap rgb to bgr
	c = 18 + width * height * 4;
	for( i = 18; i < c; i += 4 )
	{
		buffer[i] = data[i - 18 + 2];     // blue
		buffer[i + 1] = data[i - 18 + 1]; // green
		buffer[i + 2] = data[i - 18 + 0]; // red
		buffer[i + 3] = data[i - 18 + 3]; // alpha
	}

	f = fopen( filename, "wb" );
	fwrite( buffer, 1, c, f );
	fclose( f );

	free( buffer );
}

/*
============================================================================

FTX IMAGE

============================================================================
*/

typedef struct ftx_s
{
	int width;
	int height;
	int has_alpha;
	// data follows
} ftx_t;

/*
=============
LoadFTX
=============
*/
void LoadFTX( char *name, byte **pixels, int *width, int *height )
{
	FILE  *fp;
	ftx_t header;
	byte  *buffer;

	buffer = NULL;

	// read in the ftx file
	fp = SafeOpenRead( name );
	SafeRead( fp, &header, sizeof( header ));

	*width = header.width;
	*height = header.height;
	image_has_alpha = header.has_alpha;

	buffer = malloc( *width * *height * 4 );
	*pixels = buffer;
	SafeRead( fp, buffer, *width * *height * 4 );
	fclose( fp );
}

/*
============================================================================

LOAD32BITIMAGE

============================================================================
*/

/*
==============
Load32BitImage

Any of the return pointers can be NULL if you don't want them.
==============
*/
void Load32BitImage( char *name, unsigned **pixels, int *width, int *height, qboolean *has_alpha )
{
	char ext[128];
	byte *palette;
	byte *pixels8;
	byte *pixels32;
	int  size;
	int  i;
	int  v;

	image_has_alpha = qfalse;

	ExtractFileExtension( name, ext );
	if( !Q_stricmp( ext, "tga" ))
	{
		LoadTGA( name, (byte **)pixels, width, height );
	}
	else
	{
		Load256Image( name, &pixels8, &palette, width, height );
		if( !pixels )
		{
			return;
		}
		size = *width * *height;
		pixels32 = malloc( size * 4 );
		*pixels = (unsigned *)pixels32;
		for( i = 0; i < size; i++ )
		{
			v = pixels8[i];
			pixels32[i * 4 + 0] = palette[ v * 3 + 0 ];
			pixels32[i * 4 + 1] = palette[ v * 3 + 1 ];
			pixels32[i * 4 + 2] = palette[ v * 3 + 2 ];
			pixels32[i * 4 + 3] = 0xff;
		}
	}

	if( !*((byte **)pixels ))
	{
		return;
	}

	if( has_alpha )
	{
		//
		// verify that it does indeed have alpha
		//
		if( image_has_alpha )
		{
			size = *width * *height;
			pixels8 = (byte *) ( *pixels );
			for( i = 0; i < size; i++, pixels8 += 4 )
			{
				if( pixels8[ 3 ] < 0xff )
				{
					break;
				}
			}
			// it doesn't really have alpha afterall
			if( i == size )
			{
				image_has_alpha = qfalse;
			}
		}
		*has_alpha = image_has_alpha;
	}
}
