/*
 * 	Copyright (C) 2002 Thomas Schumm <pansi@phong.org>
 * 	Modifications 2012 D Haley
 *	This program is free software: you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation, either version 3 of the License, or
 *	(at your option) any later version.

 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.

 *	You should have received a copy of the GNU General Public License
 *	along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef __cplusplus 
	extern "C" { 
#endif
        
#include "pngread.h"

#include <stdlib.h>

int check_if_png(const char *file_name, FILE **fp, unsigned int bytes_to_check)
{
   char buf[bytes_to_check];

   /* Open the prospective PNG file. */
   if ((*fp = fopen(file_name, "rb")) == NULL)
      return 0;

   /* Read in some of the signature bytes */
   if (fread(buf, 1, bytes_to_check, *fp) != bytes_to_check)
      return 0;

   /* Compare the first PNG_BYTES_TO_CHECK bytes of the signature.
      Return nonzero (true) if they match */


   return(!png_sig_cmp((png_byte*)buf, (png_size_t)0, bytes_to_check));
}

/* Read a PNG file. Returns 0 on success. Must destroy output with 
 * free_pngrowpointers
 */
int read_png(FILE *fp, unsigned int sig_read, png_bytep **row_pointers,
             png_uint_32 *width, png_uint_32 *height)  /* file is already open */
{
   png_structp png_ptr;
   png_infop info_ptr;
/*   png_uint_32 width, height; */
   int bit_depth, color_type, interlace_type;

   /* Create and initialize the png_struct with the desired error handler
    * functions.  If you want to use the default stderr and longjump method,
    * you can supply NULL for the last three parameters.  We also supply the
    * the compiler header file version, so that we know if the application
    * was compiled with a compatible version of the library.  REQUIRED
    */
   png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

   if (png_ptr == NULL)
   {
      fclose(fp);
      return (-1);
   }

   /* Allocate/initialize the memory for image information.  REQUIRED. */
   info_ptr = png_create_info_struct(png_ptr);
   if (info_ptr == NULL)
   {
      fclose(fp);
      png_destroy_read_struct(&png_ptr, NULL, NULL);
      return (-1);
   }

   /* Set error handling if you are using the setjmp/longjmp method (this is
    * the normal method of doing things with libpng).  REQUIRED unless you
    * set up your own error handlers in the png_create_read_struct() earlier.
    */

   if (setjmp(png_jmpbuf(png_ptr)))
   {
      /* Free all of the memory associated with the png_ptr and info_ptr */
      png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
      fclose(fp);
      /* If we get here, we had a problem reading the file */
      return (-1);
   }

   /* One of the following I/O initialization methods is REQUIRED */
   /* Set up the input control if you are using standard C streams */
   png_init_io(png_ptr, fp);

   /* If we have already read some of the signature */
   png_set_sig_bytes(png_ptr, sig_read);

   /* OK, you're doing it the hard way, with the lower-level functions */

   /* The call to png_read_info() gives us all of the information from the
    * PNG file before the first IDAT (image data chunk).  REQUIRED
    */
   png_read_info(png_ptr, info_ptr);

   /* png_get_IHDR pulls useful data out of info_ptr. */
   png_get_IHDR(png_ptr, info_ptr, width, height, &bit_depth, &color_type,
       &interlace_type, NULL, NULL);

/* Set up the data transformations you want.  Note that these are all
 * optional.  Only call them if you want/need them.  Many of the
 * transformations only work on specific types of images, and many
 * are mutually exclusive.
 */

   /* tell libpng to strip 16 bit/color files down to 8 bits/color */
   if (bit_depth == 16)
     png_set_strip_16(png_ptr);

   /* Extract multiple pixels with bit depths of 1, 2, and 4 from a single
    * byte into separate bytes (useful for paletted and grayscale images).
    */
   if (bit_depth < 8)
     png_set_packing(png_ptr);

   /* Expand paletted colors into true RGB triplets */
   if (color_type == PNG_COLOR_TYPE_PALETTE)
      png_set_palette_to_rgb(png_ptr);

   /* Expand grayscale images to the full 8 bits from 1, 2, or 4 bits/pixel */
   if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8)
   {
	//From libpng-manual.txt:
	//
	// The function png_set_expand_gray_1_2_4_to_8() was added at libpng-1.2.9.
	// ...
	// The png_set_gray_1_2_4_to_8() function is deprecated.
	// ..
	// If you need to support versions prior to libpng-1.5.4 test the version number
	//  as illustrated below using "PNG_LIBPNG_VER >= 10504" 
#if PNG_LIBPNG_VER >= 10209
	png_set_expand_gray_1_2_4_to_8(png_ptr);
#else
	png_set_gray_1_2_4_to_8(png_ptr);
#endif
	}
	
   /* Expand paletted or RGB images with transparency to full alpha for RGBA */
   if (png_get_valid(png_ptr, info_ptr, PNG_INFO_tRNS))
      png_set_tRNS_to_alpha(png_ptr);
   /* Expand grayscale images to RGB */
   if (color_type == PNG_COLOR_TYPE_GRAY ||
       color_type == PNG_COLOR_TYPE_GRAY_ALPHA)
      png_set_gray_to_rgb(png_ptr);

   /* Strip alpha bytes from the input data without combining with the
    * background (not recommended).
   png_set_strip_alpha(png_ptr);
    */

   /* Set the background color to draw transparent and alpha images over.
    * It is possible to set the red, green, and blue components directly
    * for paletted images instead of supplying a palette index.  Note that
    * even if the PNG file supplies a background, you are not required to
    * use it - you should use the (solid) application background if it has one.
    */

/*   png_color_16 my_background; */ /*, *image_background; */
   /* use black for now */
/*    my_background.red = my_background.green = my_background.blue = 0; */

/*   if (png_get_bKGD(png_ptr, info_ptr, &image_background))
      png_set_background(png_ptr, image_background,
                         PNG_BACKGROUND_GAMMA_FILE, 1, 1.0);
   else */
     /* use black by default */
/*       png_set_background(png_ptr, &my_background, */
/*                          PNG_BACKGROUND_GAMMA_SCREEN, 0, 1.0); */

   /* Some suggestions as to how to get a screen gamma value */

   /* Note that screen gamma is the display_exponent, which includes
    * the CRT_exponent and any correction for viewing conditions */
   /* this should be user configurable somehow... */
   double screen_gamma = 2.2;  /* A good guess for a PC monitors in a dimly
                                  lit room */
   /* screen_gamma = 1.7;  or 1.0; */  /* A good guess for Mac systems */

   /* Tell libpng to handle the gamma conversion for you.  The final call
    * is a good guess for PC generated images, but it should be configurable
    * by the user at run time by the user.  It is strongly suggested that
    * your application support gamma correction.
    * 0.45455 is a "guess" for images with no particular gamma
    */

   int intent;

   if (png_get_sRGB(png_ptr, info_ptr, &intent))
      png_set_gamma(png_ptr, screen_gamma, 0.45455);
   else
   {
      double image_gamma;
      if (png_get_gAMA(png_ptr, info_ptr, &image_gamma))
         png_set_gamma(png_ptr, screen_gamma, image_gamma);
      else
         png_set_gamma(png_ptr, screen_gamma, 0.45455);
   }

   png_set_filler(png_ptr, 0xff, PNG_FILLER_AFTER);

   /* Optional call to gamma correct and add the background to the palette
    * and update info structure.  REQUIRED if you are expecting libpng to
    * update the palette for you (ie you selected such a transform above).
    */
   png_read_update_info(png_ptr, info_ptr);

   /* Allocate the memory to hold the image using the fields of info_ptr. */

   /* The easiest way to read the image: */
   if (!(*row_pointers = (png_byte**)malloc(*height * sizeof(png_bytep)))) {
     return (-1);
   }
   unsigned int row;

   for (row = 0; row < *height; row++)
   {
      (*row_pointers)[row] = (png_byte*)malloc(png_get_rowbytes(png_ptr,info_ptr));
   }

   /* Read the entire image in one go */
   png_read_image(png_ptr, *row_pointers);

   /* read rest of file, and get additional chunks in info_ptr - REQUIRED */
   png_read_end(png_ptr, info_ptr);
   /* At this point you have read the entire image */

   /* clean up after the read, and free any memory allocated - REQUIRED */
   png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

   /* close the file */
   fclose(fp);

   /* that's it */
   return (0);
}

void free_pngrowpointers(png_bytep *row_pointers, png_uint_32 height)  
{
   unsigned int row;
   for (row = 0; row < height; row++)
	free(row_pointers[row]); //FIXME : Should this be png_free ?? 
   
   free(row_pointers);
}

#ifdef __cplusplus 
	}; 
#endif
