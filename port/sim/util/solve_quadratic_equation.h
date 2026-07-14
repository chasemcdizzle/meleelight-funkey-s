// solve_quadratic_equation.h <- src/main/util/solveQuadraticEquation.js
// (structure-parallel translation). Returns null | number -> MaybeNum.
#ifndef ML_SOLVE_QUADRATIC_EQUATION_H
#define ML_SOLVE_QUADRATIC_EQUATION_H

#include "../ml_js.h"
#include "find_smallest_within.h" // MaybeNum

// solves the quadratic equation a0 + a1 x + a2 x^2 = 0
// uses the sign to choose the solution
// returns null if there are no solutions, or if the solutions are non-real
static inline MaybeNum solveQuadraticEquation(double a0, double a1, double a2,
                                              double sign) {
  if (a1 == 0 && a2 == 0) {
    if (a0 == 0) {
      return maybe_num(-1); // convention
    } else {
      return maybe_null();
    }
  } else if (js_abs(a0 * a0 * a2 / (a1 * a1)) < 1e-20) {
    return maybe_num(-a0 / a1);
  } else {
    const double disc = a1 * a1 - 4 * a0 * a2;
    if (disc < 0) {
      return maybe_null(); // non-real solutions
    } else if (js_sign(a1) == sign) {
      // avoid catastrophic cancellation
      return maybe_num(2 * a0 / (-a1 - sign * sqrt(disc)));
    } else {
      return maybe_num((-a1 + sign * sqrt(disc)) / (2 * a2));
    }
  }
}

#endif // ML_SOLVE_QUADRATIC_EQUATION_H
