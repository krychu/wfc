#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../stb_image.h"
#include "../stb_image_write.h"
#define WFC_IMPLEMENTATION
#define WFC_USE_STB
#include "../wfc.h"
#include <stdio.h>

int main(void)
{
  const char *input_filename = "../samples/square.png";
  const char *output_filename = "square.png";

  struct wfc_image *input_image = wfc_img_load(input_filename);
  if (input_image == NULL) {
    printf("Error loading file: %s\n", input_filename);
    return EXIT_FAILURE;
  }

  struct wfc *wfc = wfc_overlapping(
                                    128,
                                    128,
                                    input_image,
                                    3,
                                    3,
                                    1,
                                    1,
                                    1,
                                    1
                                    );

  if (!wfc) {
    printf("Error creating wfc\n");
    wfc_img_destroy(input_image);
    return EXIT_FAILURE;
  }

  printf("Running ... \n");
  if (!wfc_run(wfc, -1)) {
    printf("Contradiction occurred, try again\n");
    wfc_img_destroy(input_image);
    wfc_destroy(wfc);
    return EXIT_FAILURE;
  }

  struct wfc_image *output_image = wfc_output_image(wfc);
  int rv = EXIT_SUCCESS;
  if (!wfc_img_save(output_image, output_filename)) {
    printf("Error saving file: %s", output_filename);
    rv = EXIT_FAILURE;
  }

  wfc_img_destroy(input_image);
  wfc_img_destroy(output_image);
  wfc_destroy(wfc);

  return rv;
}
