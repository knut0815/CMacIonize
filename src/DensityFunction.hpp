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
 * @file DensityFunction.hpp
 *
 * @brief Interface for functors that can be used to fill a DensityGrid.
 *
 * @author Bert Vandenbroucke (bv7@st-andrews.ac.uk)
 */
#ifndef DENSITYFUNCTION_HPP
#define DENSITYFUNCTION_HPP

#include "CoordinateVector.hpp"
#include "DensityValues.hpp"

/**
 * @brief Interface for functors that can be used to fill a DensityGrid.
 */
class DensityFunction {
public:
  virtual ~DensityFunction() {}

  /**
   * @brief Function that gives the density for a given coordinate.
   *
   * @param position CoordinateVector specifying a coordinate position (in m).
   * @return Density at the given coordinate (in m^-3).
   */
  virtual DensityValues operator()(CoordinateVector<> position) const = 0;
};

#endif // DENSITYFUNCTION_HPP
