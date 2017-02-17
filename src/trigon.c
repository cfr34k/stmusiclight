#include <stdint.h>

#include "trigon.h"
#include "constants.h"

union IntFloat {
  float f;
  uint32_t u;
};

float sincos_base(float x);

#define HORNER_COEF_COUNT 5

/*
 * Approximate sin(x) in [0, pi/2] using Taylor approximation of order 10
 * around 0. For efficient calculation a horner scheme is used.
 *
 * Matlab reference code:
 * sin(x) =~ x .* (1+x.^2 .* (-(1/6)+x.^2 .* (1/120+x.^2 .* (-(1/5040)+x.^2/362880))));
 */
float sincos_base(float x) {
  // return input value for small values (max. error: 2.0833e-08)
  // FIXME: calculate value for minimum error
  if(x < 0.005f) {
    return x;
  }

  // return 1 for values close to the peak
  if(x > (PI/2 - 0.001f)) {
    return 1;
  }

  float x_sq = x*x;

  const float HORNER_COEF[HORNER_COEF_COUNT] = {
    1, (-1./6), 1./120, -(1./5040), 1./362880
  };

  float y = HORNER_COEF[HORNER_COEF_COUNT-1];

  for(int i = HORNER_COEF_COUNT-2; i >= 0; i--) {
    y = (HORNER_COEF[i] + x_sq * y);
  }

  return x*y; // the remaining y
}

float absf(float x) {
  union IntFloat conv = {x};
  conv.u &= 0x7FFFFFFF;
  return conv.f;
}

int signf(float x) {
  union IntFloat conv = {x};
  return (conv.u & 0x80000000) ? -1 : 1;
}

float sinf(float x) {
  int s = signf(x);
  x = absf(x);

  // normalize to [0, 2*pi]
  int n = x / (2*PI);
  x -= n * (2*PI);

  if(x < PI/2) {
    return s * sincos_base(x);
  } else if(x < PI) {
    return s * sincos_base(PI - x);
  } else if(x < 3*PI/2) {
    return -s * sincos_base(x - PI);
  } else {
    return -s * sincos_base(2*PI - x);
  }
}

float cosf(float x) {
  return sinf(x + PI/2);
}

float tanf(float x) {
  return sinf(x) / cosf(x);
}
