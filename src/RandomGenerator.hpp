/*******************************************************************************
 * This file is part of CMacIonize
 * Copyright (C) 2016 Bert Vandenbroucke (bert.vandenbroucke@gmail.com)
 *
 * CMacIonize is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CMacIonize is distributed in the hope that it will be useful,
 * but WITOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with CMacIonize. If not, see <http://www.gnu.org/licenses/>.
 ******************************************************************************/

/**
 * @file RandomGenerator.hpp
 *
 * @brief Custom random number generator.
 *
 * @author Bert Vandenbroucke (bv7@st-andrews.ac.uk)
 */
#ifndef RANDOMGENERATOR_HPP
#define RANDOMGENERATOR_HPP

/**
 * @brief Own implementation of the GSL ranlxs2 random generator.
 *
 * Based on http://git.savannah.gnu.org/cgit/gsl.git/tree/rng/ranlxs.c.
 */
class RandomGenerator {
private:
  /*! @brief ranlxs2 state variables. */
  double _xdbl[12];

  /*! @brief ranlxs2 state variables. */
  double _ydbl[12];

  /*! @brief ranlxs2 state variables. */
  double _carry;

  /*! @brief ranlxs2 state variables. */
  float _xflt[24];

  /*! @brief ranlxs2 state variables. */
  unsigned int _ir;

  /*! @brief ranlxs2 state variables. */
  unsigned int _jr;

  /*! @brief ranlxs2 state variables. */
  unsigned int _is;

  /*! @brief ranlxs2 state variables. */
  unsigned int _is_old;

  /*! @brief ranlxs2 state variables. */
  unsigned int _pr;

  /**
   * @brief GSL RANLUX_STEP macro.
   *
   * @param xdbl Pointer to the xdbl array.
   * @param x1 Reference to either y1, y2 or y3 in increment_state.
   * @param x2 Reference to either y1, y2 or y3 in increment_state.
   * @param i1 Index in the xdbl array.
   * @param i2 Index in the xdbl array.
   * @param i3 Index in the xdbl array.
   */
  static inline void ranlux_step(double *xdbl, double &x1, double &x2,
                                 unsigned int i1, unsigned int i2,
                                 unsigned int i3) {
    x1 = xdbl[i1] - xdbl[i2];
    if (x2 < 0) {
      x1 -= (1.0 / 281474976710656.0);
      x2 += 1;
    }
    xdbl[i3] = x2;
  }

  /**
   * @brief Increment the internal state of the generator.
   */
  inline void increment_state() {
    int k, kmax, m;
    double x, y1, y2, y3;

    float *xflt = _xflt;
    double *xdbl = _xdbl;
    double *ydbl = _ydbl;
    double carry = _carry;
    unsigned int ir = _ir;
    unsigned int jr = _jr;

    for (k = 0; ir > 0; ++k) {
      y1 = xdbl[jr] - xdbl[ir];
      y2 = y1 - carry;
      if (y2 < 0) {
        carry = (1.0 / 281474976710656.0);
        y2 += 1;
      } else {
        carry = 0;
      }
      xdbl[ir] = y2;
      ir = (ir + 1) % 12;
      jr = (jr + 1) % 12;
    }

    kmax = _pr - 12;

    for (; k <= kmax; k += 12) {
      y1 = xdbl[7] - xdbl[0];
      y1 -= carry;

      ranlux_step(xdbl, y2, y1, 8, 1, 0);
      ranlux_step(xdbl, y3, y2, 9, 2, 1);
      ranlux_step(xdbl, y1, y3, 10, 3, 2);
      ranlux_step(xdbl, y2, y1, 11, 4, 3);
      ranlux_step(xdbl, y3, y2, 0, 5, 4);
      ranlux_step(xdbl, y1, y3, 1, 6, 5);
      ranlux_step(xdbl, y2, y1, 2, 7, 6);
      ranlux_step(xdbl, y3, y2, 3, 8, 7);
      ranlux_step(xdbl, y1, y3, 4, 9, 8);
      ranlux_step(xdbl, y2, y1, 5, 10, 9);
      ranlux_step(xdbl, y3, y2, 6, 11, 10);

      if (y3 < 0) {
        carry = (1.0 / 281474976710656.0);
        y3 += 1;
      } else {
        carry = 0;
      }
      xdbl[11] = y3;
    }

    kmax = _pr;

    for (; k < kmax; ++k) {
      y1 = xdbl[jr] - xdbl[ir];
      y2 = y1 - carry;
      if (y2 < 0) {
        carry = (1.0 / 281474976710656.0);
        y2 += 1;
      } else {
        carry = 0;
      }
      xdbl[ir] = y2;
      ydbl[ir] = y2 + 268435456.0;
      ir = (ir + 1) % 12;
      jr = (jr + 1) % 12;
    }

    ydbl[ir] = xdbl[ir] + 268435456.0;

    for (k = (ir + 1) % 12; k > 0;) {
      ydbl[k] = xdbl[k] + 268435456.0;
      k = (k + 1) % 12;
    }

    for (k = 0, m = 0; k < 12; ++k) {
      x = xdbl[k];
      y2 = ydbl[k] - 268435456.0;
      if (y2 > x)
        y2 -= (1.0 / 16777216.0);
      y1 = (x - y2) * 16777216.0;

      xflt[m++] = (float)y1;
      xflt[m++] = (float)y2;
    }

    _ir = ir;
    _is = 2 * ir;
    _is_old = 2 * ir;
    _jr = jr;
    _carry = carry;
  }

public:
  /**
   * @brief Set a new seed for the random generator.
   *
   * @param seed New seed.
   */
  inline void set_seed(int seed) {
    int ibit, jbit, i, k, m, xbit[31];
    double x, y;

    if (seed == 0) {
      // the default seed is 1, not 0
      seed = 1;
    }

    // Allowed seeds for ranlxs are 0 .. 2^31-1
    i = seed & 0x7FFFFFFFUL;

    for (k = 0; k < 31; ++k) {
      xbit[k] = i % 2;
      i /= 2;
    }

    ibit = 0;
    jbit = 18;

    for (k = 0; k < 12; ++k) {
      x = 0;

      for (m = 1; m <= 48; ++m) {
        y = (double)xbit[ibit];
        x += x + y;
        xbit[ibit] = (xbit[ibit] + xbit[jbit]) % 2;
        ibit = (ibit + 1) % 31;
        jbit = (jbit + 1) % 31;
      }
      _xdbl[k] = (1.0 / 281474976710656.0) * x;
    }

    _carry = 0;
    _ir = 0;
    _jr = 7;
    _is = 23;
    _is_old = 0;
    // we implement the ranlxs2 generator
    _pr = 397;
  }

  /**
   * @brief Constructor.
   *
   * @param seed Initial seed for the random number generator.
   */
  inline RandomGenerator(int seed = 42) { set_seed(seed); }

  /**
   * @brief Get a uniform random double precision floating point value in the
   * range [0., 1.].
   *
   * Note that this function changes the internal state of the RandomGenerator.
   *
   * @return Random double precision floating point value.
   */
  inline double get_uniform_random_double() {
    _is = (_is + 1) % 24;

    if (_is == _is_old) {
      increment_state();
    }

    return _xflt[_is];
  }

  /**
   * @brief Get a random integer value.
   *
   * @return Random integer value in the range [0, 2^24].
   */
  inline int get_random_integer() {
    return get_uniform_random_double() * 16777216.0;
  }
};

#endif // RANDOMGENERATOR_HPP
