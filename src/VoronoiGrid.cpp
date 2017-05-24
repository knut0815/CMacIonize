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
 * @file VoronoiGrid.cpp
 *
 * @brief VoronoiGrid implementation.
 *
 * @author Bert Vandenbroucke (bv7@st-andrews.ac.uk)
 */
#include "VoronoiGrid.hpp"
#include "Error.hpp"
#include "PointLocations.hpp"
#include "VoronoiCell.hpp"
#include "WorkDistributor.hpp"

/**
 * @brief Constructor.
 *
 * @param box Bounding box containing the entire grid.
 * @param periodic Periodicity flags for the bounding box.
 * @param numcell Number of cells that will be added to the grid (or zero if
 * unknown).
 */
VoronoiGrid::VoronoiGrid(Box box, CoordinateVector< bool > periodic,
                         unsigned int numcell)
    : _box(box), _periodic(periodic), _pointlocations(nullptr),
      _epsilon(VORONOI_TOLERANCE * _box.get_sides().norm2()) {

  _cells.reserve(numcell);

  for (unsigned int i = 0; i < 3; ++i) {
    if (_periodic[i]) {
      _box.get_anchor()[i] -= 0.5 * _box.get_sides()[i];
      _box.get_sides()[i] *= 2.;
    }
  }
}

/**
 * @brief Destructor.
 *
 * Free cell memory.
 */
VoronoiGrid::~VoronoiGrid() {
  for (unsigned int i = 0; i < _cells.size(); ++i) {
    delete _cells[i];
  }
  delete _pointlocations;
}

/**
 * @brief Reset the Voronoi grid, based on the actual generator positions.
 *
 * @param worksize Number of parallel threads to use during the grid
 * reconstruction.
 */
void VoronoiGrid::reset(int worksize) {
  for (unsigned int i = 0; i < _cells.size(); ++i) {
    delete _cells[i];
    _cells[i] = new VoronoiCell(_generator_positions[i], _box);
  }
  delete _pointlocations;
  compute_grid(worksize);
}

/**
 * @brief Add a new cell to the VoronoiGrid, using the given coordinate position
 * as generator of the cell.
 *
 * @param generator_position Coordinates of the cell generator (in m).
 * @return Index of the new cell in the internal list. This index can later be
 * used to query cell properties.
 */
unsigned int VoronoiGrid::add_cell(CoordinateVector<> generator_position) {
  if (_cells.size() + 1 == VORONOI_MAX_INDEX) {
    cmac_error("Too many Voronoi cells!");
  }
  _generator_positions.push_back(generator_position);
  _cells.push_back(new VoronoiCell(generator_position, _box));
  return _cells.size() - 1;
}

/**
 * @brief Compute the cell for the generator with the given index.
 *
 * @param index Index of the cell that should be constructed.
 */
void VoronoiGrid::compute_cell(unsigned int index) {
  auto it = _pointlocations->get_neighbours(index);
  auto ngbs = it.get_neighbours();
  for (auto ngbit = ngbs.begin(); ngbit != ngbs.end(); ++ngbit) {
    const unsigned int j = *ngbit;
    if (j != index) {
      _cells[index]->intersect(
          _generator_positions[j] - _generator_positions[index], j, _epsilon);
    }
  }
  while (it.increase_range() &&
         it.get_max_radius2() < 4. * _cells[index]->get_max_radius_squared()) {
    ngbs = it.get_neighbours();
    for (auto ngbit = ngbs.begin(); ngbit != ngbs.end(); ++ngbit) {
      const unsigned int j = *ngbit;
      _cells[index]->intersect(
          _generator_positions[j] - _generator_positions[index], j, _epsilon);
    }
  }
}

/**
 * @brief Compute the Voronoi cells of the grid.
 *
 * @param worksize Number of parallel threads to use.
 */
void VoronoiGrid::compute_grid(int worksize) {
  _pointlocations = new PointLocations(_generator_positions, 10, _box);

  WorkDistributor< VoronoiGridConstructionJobMarket,
                   VoronoiGridConstructionJob >
      workers(worksize);
  VoronoiGridConstructionJobMarket jobs(*this, 100);
  workers.do_in_parallel(jobs);
}

/**
 * @brief Finalize the cells of the grid.
 *
 * After this routine has been called, the actual grid information is lost, but
 * every cell has a volume, centroid and faces based on the grid.
 */
void VoronoiGrid::finalize() {
  double totvol = 0.;
  for (unsigned int i = 0; i < _cells.size(); ++i) {
    _cells[i]->finalize();
    totvol += _cells[i]->get_volume();
  }

  cmac_assert_message(std::abs(totvol - _box.get_volume()) <
                          1.e-12 * (totvol + _box.get_volume()),
                      "%g =/= %g  -- relative difference: %g", totvol,
                      _box.get_volume(), std::abs(totvol - _box.get_volume()) /
                                             (totvol + _box.get_volume()));
}

/**
 * @brief Get the volume of the cell with the given index.
 *
 * @param index Index of a cell in the grid.
 * @return Volume of that cell (in m^3).
 */
double VoronoiGrid::get_volume(unsigned int index) const {
  return _cells[index]->get_volume();
}

/**
 * @brief Get the centroid of the cell with the given index.
 *
 * @param index Index of a cell in the grid.
 * @return Centroid of that cell (in m).
 */
const CoordinateVector<> &VoronoiGrid::get_centroid(unsigned int index) const {
  return _cells[index]->get_centroid();
}

/**
 * @brief Get the position of the generator of the cell with the given index.
 *
 * @param index Index of a cell in the grid.
 * @return Position of the generator of that cell (in m).
 */
const CoordinateVector<> &VoronoiGrid::get_generator(unsigned int index) const {
  return _generator_positions[index];
}

/**
 * @brief Get the position of the generator of the cell with the given index.
 *
 * @param index Index of a cell in the grid.
 * @return Position of the generator of that cell (in m).
 */
CoordinateVector<> &VoronoiGrid::get_generator(unsigned int index) {
  return _generator_positions[index];
}

/**
 * @brief Get the normal of the wall with the given index.
 *
 * @param wallindex Index of a wall of the box.
 * @return Normal vector to the given wall.
 */
CoordinateVector<> VoronoiGrid::get_wall_normal(unsigned int wallindex) const {
  cmac_assert(wallindex >= VORONOI_MAX_INDEX);

  switch (wallindex) {
  case VORONOI_BOX_LEFT:
    return CoordinateVector<>(-1., 0., 0.);
  case VORONOI_BOX_RIGHT:
    return CoordinateVector<>(1., 0., 0.);
  case VORONOI_BOX_FRONT:
    return CoordinateVector<>(0., -1., 0.);
  case VORONOI_BOX_BACK:
    return CoordinateVector<>(0., 1., 0.);
  case VORONOI_BOX_BOTTOM:
    return CoordinateVector<>(0., 0., -1.);
  case VORONOI_BOX_TOP:
    return CoordinateVector<>(0., 0., 1.);
  }

  cmac_error("Not a valid wall index: %u!", wallindex);
  return CoordinateVector<>();
}

/**
 * @brief Get the faces of the cell with the given index.
 *
 * @param index Index of a cell in the grid.
 * @return std::vector containing, for each face, its surface area (in m^2), its
 * midpoint (in m), and the index of the neighbouring cell that generated the
 * face.
 */
const std::vector< VoronoiFace > &
VoronoiGrid::get_faces(unsigned int index) const {
  return _cells[index]->get_faces();
}

/**
 * @brief Get the index of the cell containing the given position.
 *
 * @param position Position (in m).
 * @return Index of the cell containing that position.
 */
unsigned int VoronoiGrid::get_index(const CoordinateVector<> &position) const {
  return _pointlocations->get_closest_neighbour(position);
}

/**
 * @brief Check if the given position is inside the box that contains the grid.
 *
 * @param position Position (in m).
 * @return True if the position is inside the grid box.
 */
bool VoronoiGrid::is_inside(CoordinateVector<> position) const {
  return _box.inside(position);
}

/**
 * @brief Print the cell with the given index to the given stream in a format
 * that can be easily plotted using gnuplot.
 *
 * @param index Index of a cell.
 * @param stream std::ostream to write to.
 */
void VoronoiGrid::print_cell(unsigned int index, std::ostream &stream) {
  _cells[index]->print_cell(stream);
}

/**
 * @brief Print the grid to the given stream in a format that can be easily
 * plotted using gnuplot.
 *
 * @param stream std::ostream to write to.
 */
void VoronoiGrid::print_grid(std::ostream &stream) {
  for (unsigned int i = 0; i < _cells.size(); ++i) {
    _cells[i]->print_cell(stream);
  }
}
