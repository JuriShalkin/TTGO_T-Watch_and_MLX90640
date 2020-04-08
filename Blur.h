//*************************************************
//  Lightweight 1:2 Gaussian interpolation for
//  small displays
//  (F) Dzl 2019
//  Optimized for simplicity and does not account for
//  screen edge ghosting.
//*************************************************
//  Image dimensions
#define IMAGE_WIDTH 32
#define IMAGE_HEIGHT 24

//  Different kernels:
//Sigma=1
#define P0 (0.077847)
#define P1 (0.123317+0.077847)
#define P2 (0.195346+0.123317+0.123317+0.077847)
/*
  //Sigma=0.5
  #define P0 (0.024879)
  #define P1 (0.107973+0.024879)
  #define P2 (0.468592+0.107973+0.107973+0.024879)
*/
/*
  //Sigma=2
  #define P0 (0.102059)
  #define P1 (0.115349+0.102059)
  #define P2 (0.130371+0.115349+0.115349+0.102059)
*/

int ktype = 1;
int offset[4][9] =
{
  { -33, -32, -32,
    -1 , 0  , 0,
    -1 , 0  , 0
  },
  { - 32, -32, -31,
    0, 0, 1,
    0, 0, 1
  },
  { - 1, 0, 0,
    -1, 0, 0,
    31, 32, 32
  },
  { 0, 0, 1,
    0, 0, 1,
    32, 32, 33
  },
};

float kernel[3][9] =
{
  //Signa=0.5
  { 0.024879,  0.107973,  0.024879,
    0.107973,  0.468592,  0.107973,
    0.024879,  0.107973,  0.024879
  },
  //Sigma = 1.0
  { 0.077847,  0.123317,  0.077847,
    0.123317,  0.195346,  0.123317,
    0.077847,  0.123317,  0.077847
  },
  //Sigma=2.0
  { 0.102059,  0.115349,  0.102059,
    0.115349,  0.130371,  0.115349,
    0.102059,  0.115349,  0.102059
  }
};

class GBlur
{
    //-Offsets
    /*
    const int offsets[4][4] =
    {
      { -IMAGE_WIDTH - 1, -IMAGE_WIDTH, -1, 0},
      { -IMAGE_WIDTH, -IMAGE_WIDTH + 1, 0, 1},
      { -1, 0, IMAGE_WIDTH, IMAGE_WIDTH + 1},
      {0, 1, IMAGE_WIDTH, IMAGE_WIDTH + 1}
    };
    const float kernel[4][4] =
    {
      {P0, P1, P1, P2},
      {P1, P0, P2, P1},
      {P1, P2, P0, P1},
      {P2, P1, P1, P0}
    };
    */
  public:
    //*************************************************
    //  This method takes two pixel arrays:
    //  'source' and 'dest'.
    //  For speed 'IMAGE_WIDTH' and 'IMAGE_HEIGHT'
    //  (original image size) are pre defiend in the top.
    //  'source' is the original (monochrome) pixels
    //  and 'dest' is the interpolated pixels.
    //  the size of 'source' is IMAGE_WIDTH*IMAGE_HEIGHT
    //  and 'dest' is IMAGE_WIDTH*IMAGE_HEIGHT*4
    //*************************************************
    void calculate(float *source, float *dest)
    {
      float pix;
      //For rest of  output pixel:
      for (int i = 0; i < IMAGE_WIDTH * IMAGE_HEIGHT * 4; i++)
      {
        int sourceAddress = ((i >> 1) & 0x1f) + ((i & 0xffffff80) >> 2);
        pix = 0;
        int q = (i & 0x00000001) + ((i & 0x00000040) >> 5);   //Calculation to perform
        for (int z = 0; z < 9; z++)
        {
          int sa = sourceAddress + offset[q][z];
          if (sa > 0 && sa < IMAGE_WIDTH * IMAGE_HEIGHT)
            pix += kernel[ktype][z] * source[sa];
          dest[i] = pix;
        }
      }
    }
};
