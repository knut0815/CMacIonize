/*******************************************************************************
 * This file is part of CMacIonize
 * Copyright (C) 2017 Bert Vandenbroucke (bert.vandenbroucke@gmail.com)
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
 * @file VoronoiFace.hpp
 *
 * @brief Face of the Voronoi grid.
 *
 * @author Bert Vandenbroucke (bv7@st-andrews.ac.uk)
 */
#ifndef VORONOIFACE_HPP
#define VORONOIFACE_HPP

#include "CoordinateVector.hpp"

/**
 * @brief Face of the Voronoi grid.
 */
class VoronoiFace {
private:
  /*! @brief Surface area of the face (in m^2). */
  double _surface_area;

  /*! @brief Midpoint of the face (in m). */
  CoordinateVector<> _midpoint;

  /*! @brief Neighbour of the face. */
  unsigned int _neighbour;

public:
  /**
   * @brief (Empty) constructor.
   */
  inline VoronoiFace() : _surface_area(0.), _midpoint(0.), _neighbour(0) {}

  /**
   * @brief Get the surface area of the face.
   *
   * @return Surface area of the face (in m^2).
   */
  inline double get_surface_area() const { return _surface_area; }

  /**
   * @brief Set the surface area of the face.
   *
   * @param surface_area Surface area of the face (in m^2).
   */
  inline void set_surface_area(double surface_area) {
    _surface_area = surface_area;
  }

  /**
   * @brief Get the midpoint of the face.
   *
   * @return Midpoint of the face (in m).
   */
  inline CoordinateVector<> get_midpoint() const { return _midpoint; }

  /**
   * @brief Set the midpoint of the face.
   *
   * @param midpoint Midpoint of the face (in m).
   */
  inline void set_midpoint(CoordinateVector<> midpoint) {
    _midpoint = midpoint;
  }

  /**
   * @brief Get the neighbour of the face.
   *
   * @return Neighbour of the face.
   */
  inline unsigned int get_neighbour() const { return _neighbour; }

  /**
   * @brief Set the neighbour of the face.
   *
   * @param neighbour Neighbour of the face.
   */
  inline void set_neighbour(unsigned int neighbour) { _neighbour = neighbour; }
};

#endif // VORONOIFACE_HPP
