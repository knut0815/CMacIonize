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
 * @file SPHNGSnapshotDensityFunction.cpp
 *
 * @brief SPHNGSnapshotDensityFunction implementation.
 *
 * @author Bert Vandenbroucke (bv7@st-andrews.ac.uk)
 */
#include "SPHNGSnapshotDensityFunction.hpp"
#include "DensityValues.hpp"
#include "Log.hpp"
#include "Octree.hpp"
#include "ParameterFile.hpp"
#include "UnitConverter.hpp"
#include <cfloat>
#include <fstream>
#include <map>

/**
 * @brief Cubic spline kernel.
 *
 * As in Price, 2007, Publications of the Astronomical Society of Australia, 24,
 * 159 (equation 5).
 *
 * @param q Distance between the kernel center and the evaluation point, in
 * units of the smoothing length.
 * @param h Smoothing length.
 * @return Value of the kernel.
 */
double SPHNGSnapshotDensityFunction::kernel(const double q, const double h) {
  if (q < 1.) {
    double q2 = q * q;
    double h2 = h * h;
    double h3 = h2 * h;
    return (1. - 1.5 * q2 + 0.75 * q2 * q) / M_PI / h3;
  } else if (q < 2.) {
    double c = 2. - q;
    double c2 = c * c;
    double h2 = h * h;
    double h3 = h * h2;
    return 0.25 * c2 * c / M_PI / h3;
  } else {
    return 0.;
  }
}

/**
 * @brief Constructor.
 *
 * Reads the file and stores the particle variables in internal arrays. Does not
 * construct the Octree, this is done in initialize().
 *
 * @param filename Name of the file to read.
 * @param initial_temperature Initial temperature of the gas (in K).
 * @param write_stats Flag indicating whether or not to write a file with
 * neighbour statistics.
 * @param stats_numbin Number of logarithmic bins used to keep track of
 * interneighbour distances.
 * @param stats_mindist Minimum interneighbour distance bin (in m; needs to be
 * non-zero, as we use logarithmic binning).
 * @param stats_maxdist Maximum interneighbour distance bin (in m).
 * @param stats_filename Name of the file with neighbour statistics that will be
 * written out.
 * @param log Log to write logging info to.
 */
SPHNGSnapshotDensityFunction::SPHNGSnapshotDensityFunction(
    std::string filename, double initial_temperature, bool write_stats,
    unsigned int stats_numbin, double stats_mindist, double stats_maxdist,
    std::string stats_filename, Log *log)
    : _octree(nullptr), _initial_temperature(initial_temperature),
      _stats_numbin(stats_numbin), _stats_mindist(stats_mindist),
      _stats_maxdist(stats_maxdist), _stats_filename(stats_filename),
      _log(log) {
  std::ifstream file(filename, std::ios::binary | std::ios::in);

  if (!file) {
    cmac_error("Unable to open file \"%s\"!", filename.c_str());
  }

  // read header

  // skip the first block: it contains garbage
  skip_block(file);
  // read the second block: it contains the file identity
  // we only support file identities starting with 'F'
  // there is currently no support for untagged files ('T')
  std::string fileident;
  read_block(file, fileident);
  if (fileident[0] != 'F') {
    cmac_error("Unsupported SPHNG snapshot format: %s!", fileident.c_str());
  }
  bool tagged = true;
  if (fileident[1] != 'T') {
    tagged = false;
  }

  // the third, fourth and fifth block contain a dictionary of particle numbers
  // the code below reads it
  // since we currently don't use these values, we skip these 3 blocks instead
  std::map< std::string, unsigned int > numbers =
      read_dict< unsigned int >(file, tagged);
  if (!tagged) {
    unsigned int numnumbers = numbers.size();
    numbers["nparttot"] = numbers["tag"];
    if (numnumbers == 6) {
      numbers["nblocks"] = 1;
    } else {
      numbers["nblocks"] = numbers["tag6"];
    }
  }
  unsigned int numpart = numbers["nparttot"];
  unsigned int numblock = numbers["nblocks"];
  //  skip_block(file);
  //  if (tagged) {
  //    skip_block(file);
  //  }
  //  skip_block(file);

  // skip 3 blocks
  // in the example file I got from Will, all these blocks contain a single
  // integer with value zero. They supposedly correspond to blocks that are
  // absent
  skip_block(file);
  skip_block(file);
  skip_block(file);

  // the next three blocks are a dictionary containing the highest unique index
  // in the snapshot. If the first block contains 0, then the next 2 blocks are
  // absent.
  int number;
  read_block(file, number);

  if (number == 1) {
    // skip blocks
    if (tagged) {
      skip_block(file);
    }
    skip_block(file);
  }

  // the next three blocks contain a number of double precision floating point
  // values that we don't use. They can be read in with the code below, but we
  // just skip them.
  //  std::map< std::string, double > headerdict = read_dict< double >(file);
  skip_block(file);
  if (tagged) {
    skip_block(file);
  }
  skip_block(file);

  // the next block again corresponds to a block that is absent from Will's
  // example file
  skip_block(file);

  // the next three blocks contain the units
  std::map< std::string, double > units = read_dict< double >(file, tagged);
  if (!tagged) {
    units["udist"] = units["tag"];
    units["umass"] = units["tag1"];
  }

  // the last header block is again absent from Will's file
  skip_block(file);

  // done reading header!

  _partbox.get_anchor()[0] = DBL_MAX;
  _partbox.get_anchor()[1] = DBL_MAX;
  _partbox.get_anchor()[2] = DBL_MAX;
  _partbox.get_sides()[0] = -DBL_MAX;
  _partbox.get_sides()[1] = -DBL_MAX;
  _partbox.get_sides()[2] = -DBL_MAX;

  Box<> rawunitsbox;
  rawunitsbox.get_anchor()[0] = DBL_MAX;
  rawunitsbox.get_anchor()[1] = DBL_MAX;
  rawunitsbox.get_anchor()[2] = DBL_MAX;
  rawunitsbox.get_sides()[0] = -DBL_MAX;
  rawunitsbox.get_sides()[1] = -DBL_MAX;
  rawunitsbox.get_sides()[2] = -DBL_MAX;

  double unit_length =
      UnitConverter::to_SI< QUANTITY_LENGTH >(units["udist"], "cm");
  double unit_mass = UnitConverter::to_SI< QUANTITY_MASS >(units["umass"], "g");

  _positions.reserve(numpart);
  _masses.reserve(numpart);
  _smoothing_lengths.reserve(numpart);
  //  std::vector<unsigned long> all_iunique;
  //  all_iunique.reserve(numpart);

  // read blocks
  for (unsigned int iblock = 0; iblock < numblock; ++iblock) {
    unsigned long npart;
    std::vector< unsigned int > nums(8);
    read_block(file, npart, nums);

    unsigned long nptmass;
    std::vector< unsigned int > numssink(8);
    read_block(file, nptmass, numssink);

    if (tagged) {
      skip_block(file);
    }

    std::vector< int > isteps(npart);
    read_block(file, isteps);

    if (nums[0] >= 2) {
      // skip 2 blocks
      if (tagged) {
        skip_block(file);
      }
      skip_block(file);
    }

    std::string tag;
    if (tagged) {
      read_block(file, tag);
    } else {
      tag = "iphase";
    }

    if (tag != "iphase") {
      cmac_error("Wrong tag: \"%s\" (expected \"iphase\")!", tag.c_str());
    }

    std::vector< char > iphase(npart);
    read_block(file, iphase);

    //    std::vector<unsigned long> iunique(npart);
    if (nums[4] >= 1) {
      // skip iunique block
      if (tagged) {
        skip_block(file);
      }
      skip_block(file);
      //      if(tagged){
      //        skip_block(file);
      //      }
      //      read_block(file, iunique);
    }

    std::vector< double > x(npart);
    if (tagged) {
      skip_block(file);
    }
    read_block(file, x);

    std::vector< double > y(npart);
    if (tagged) {
      skip_block(file);
    }
    read_block(file, y);

    std::vector< double > z(npart);
    if (tagged) {
      skip_block(file);
    }
    read_block(file, z);

    std::vector< double > m(npart);
    if (tagged) {
      skip_block(file);
    }
    read_block(file, m);

    std::vector< double > h(npart);
    if (tagged) {
      skip_block(file);
    }
    read_block(file, h);

    // skip velocity, thermal energy and density blocks
    for (unsigned int i = 0; i < 5; ++i) {
      if (tagged) {
        skip_block(file);
      }
      skip_block(file);
    }

    // skip igrad related blocks
    for (unsigned int i = 0; i < nums[6] - 1; ++i) {
      if (tagged) {
        skip_block(file);
      }
      skip_block(file);
    }

    // skip sink particle data
    for (unsigned int i = 0; i < 10; ++i) {
      if (tagged) {
        skip_block(file);
      }
      skip_block(file);
    }

    for (unsigned int i = 0; i < npart; ++i) {
      if (iphase[i] == 0) {
        CoordinateVector<> rawunitsposition(x[i], y[i], z[i]);
        rawunitsbox.get_anchor() =
            CoordinateVector<>::min(rawunitsbox.get_anchor(), rawunitsposition);
        rawunitsbox.get_sides() =
            CoordinateVector<>::max(rawunitsbox.get_sides(), rawunitsposition);
        CoordinateVector<> position(x[i] * unit_length, y[i] * unit_length,
                                    z[i] * unit_length);
        _positions.push_back(position);
        _partbox.get_anchor() =
            CoordinateVector<>::min(_partbox.get_anchor(), position);
        _partbox.get_sides() =
            CoordinateVector<>::max(_partbox.get_sides(), position);
        _masses.push_back(m[i] * unit_mass);
        _smoothing_lengths.push_back(h[i] * unit_length);
        //        all_iunique.push_back(iunique[i]);
      }
    }
  }

  // done reading file
  // we would like to reduce memory usage at this point by shrinking the
  // internal vectors, using shrink_to_fit. However, the Intel compiler for some
  // reason complains about not finding this function (while still supporting
  // all other C++11 features), so we disable it here.
  // Note that enabling the code below might reduce the memory imprint,
  // especially if large snapshots are read in (if you use a suitable compiler,
  // that is).
  // BEGIN OF CODE
  // _positions.shrink_to_fit();
  // _masses.shrink_to_fit();
  // _smoothing_lengths.shrink_to_fit();
  // END OF CODE

  _partbox.get_sides() -= _partbox.get_anchor();
  // add some margin to the box
  _partbox.get_anchor() -= 0.01 * _partbox.get_sides();
  _partbox.get_sides() *= 1.02;

  if (_log) {
    _log->write_status("Snapshot contains ", _positions.size(),
                       " gas particles.");
    _log->write_status(
        "Will create octree in box with anchor [", _partbox.get_anchor().x(),
        " m, ", _partbox.get_anchor().y(), " m, ", _partbox.get_anchor().z(),
        " m] and sides [", _partbox.get_sides().x(), " m, ",
        _partbox.get_sides().y(), " m, ", _partbox.get_sides().z(), " m]...");
    _log->write_info(
        "In raw units, this corresponds to a box with anchor [",
        rawunitsbox.get_anchor().x(), ", ", rawunitsbox.get_anchor().y(), ", ",
        rawunitsbox.get_anchor().z(), "], and sides [",
        rawunitsbox.get_sides().x(), ", ", rawunitsbox.get_sides().y(), ", ",
        rawunitsbox.get_sides().z(), "].");
  }

  if (!write_stats) {
    _stats_numbin = 0;
  }
}

/**
 * @brief ParameterFile constructor.
 *
 * @param params ParameterFile to read from.
 * @param log Log to write logging info to.
 */
SPHNGSnapshotDensityFunction::SPHNGSnapshotDensityFunction(
    ParameterFile &params, Log *log)
    : SPHNGSnapshotDensityFunction(
          params.get_value< std::string >("densityfunction:filename"),
          params.get_physical_value< QUANTITY_TEMPERATURE >(
              "densityfunction:initial_temperature", "8000. K"),
          params.get_value< bool >("densityfunction:write_statistics", false),
          params.get_value< unsigned int >(
              "densityfunction:statistics_number_of_bins", 200),
          params.get_physical_value< QUANTITY_LENGTH >(
              "densityfunction:statistics_minimum_distance", "1.e-5 m"),
          params.get_physical_value< QUANTITY_LENGTH >(
              "densityfunction:statistics_maximum_distance", "1. kpc"),
          params.get_value< std::string >("densityfunction:statistics_filename",
                                          "ngb_statistics.txt"),
          log) {}

/**
 * @brief Destructor.
 *
 * Clean up the octree.
 */
SPHNGSnapshotDensityFunction::~SPHNGSnapshotDensityFunction() {
  delete _octree;
}

/**
 * @brief This routine constructs the internal Octree that is used for neighbour
 * finding.
 */
void SPHNGSnapshotDensityFunction::initialize() {
  _octree = new Octree(_positions, _partbox, false);
  _octree->set_auxiliaries(_smoothing_lengths, Octree::max< double >);

  if (_stats_numbin > 0) {
    if (_log) {
      _log->write_status("Obtaining particle neighbour statistics...");
    }
    const unsigned int numbin = _stats_numbin;
    const double mindist = _stats_mindist;
    const double maxdist = _stats_maxdist;
    const double logmindist = std::log10(mindist);
    const double logmaxdist = std::log10(maxdist);
    const double logddist = logmaxdist - logmindist;
    std::vector< double > ngbstats(numbin);
    double totnumngb = 0.;
    double numsmall = 0.;
    double numlarge = 0.;
    for (unsigned int i = 0; i < _positions.size(); ++i) {
      if (_log && i % (_positions.size() / 10) == 0) {
        _log->write_info("Got statistics for ", i, " of ", _positions.size(),
                         " particles.");
      }
      std::vector< unsigned int > ngbs = _octree->get_ngbs(_positions[i]);
      const unsigned int numngbs = ngbs.size();
      totnumngb += numngbs;
      for (unsigned int j = 0; j < numngbs; ++j) {
        unsigned int index = ngbs[j];
        if (index != i) {
          double r = (_positions[i] - _positions[index]).norm();
          if (r >= mindist) {
            unsigned int ibin =
                (std::log10(r) - logmindist) / logddist * numbin;
            if (ibin < numbin) {
              ngbstats[ibin] += 1.;
            } else {
              numlarge += 1.;
            }
          } else {
            numsmall += 1.;
          }
        }
      }
    }

    std::ofstream statfile(_stats_filename);
    double numinside = totnumngb - numsmall - numlarge;
    statfile << "# statistics account for " << (numinside / totnumngb) * 100.
             << "% of the particles.\n";
    statfile << "# " << (numsmall / totnumngb) * 100.
             << "% of the particles was closer together, "
             << (numlarge / totnumngb) * 100. << "% was further apart.\n";
    statfile << "#\n# r\tfraction\n";
    for (unsigned int i = 0; i < numbin; ++i) {
      double r = std::pow(10., i * logddist / numbin + logmindist);
      statfile << r << "\t" << ngbstats[i] / totnumngb << "\n";
    }
    if (_log) {
      _log->write_status("Wrote statistics file \"", _stats_filename, "\".");
    }
  }
}

/**
 * @brief Get the position of the particle with the given index.
 *
 * @param index Index of a particle.
 * @return CoordinateVector<> containing the position of that particle (in m).
 */
CoordinateVector<>
SPHNGSnapshotDensityFunction::get_position(unsigned int index) {
  return _positions[index];
}

/**
 * @brief Get the mass of the particle with the given index.
 *
 * @param index Index of a particle.
 * @return Mass of the particle (in kg).
 */
double SPHNGSnapshotDensityFunction::get_mass(unsigned int index) {
  return _masses[index];
}

/**
 * @brief Get the smoothing length of the particle with the given index.
 *
 * @param index Index of a particle.
 * @return Smoothing length of the particle (in m).
 */
double SPHNGSnapshotDensityFunction::get_smoothing_length(unsigned int index) {
  return _smoothing_lengths[index];
}

/**
 * @brief Function that gives the density for a given cell.
 *
 * @param cell Geometrical information about the cell.
 * @return Initial physical field values for that cell.
 */
DensityValues SPHNGSnapshotDensityFunction::operator()(const Cell &cell) const {
  DensityValues values;

  const CoordinateVector<> position = cell.get_cell_midpoint();

  double density = 0.;
  std::vector< unsigned int > ngbs = _octree->get_ngbs(position);
  const unsigned int numngbs = ngbs.size();
  for (unsigned int i = 0; i < numngbs; ++i) {
    const unsigned int index = ngbs[i];
    const double r = (position - _positions[index]).norm();
    const double h = _smoothing_lengths[index];
    const double q = r / h;
    const double m = _masses[index];
    const double splineval = m * kernel(q, h);
    density += splineval;
  }

  // convert density to particle density (assuming hydrogen only)
  values.set_number_density(density / 1.6737236e-27);
  // TODO: other quantities
  // temporary values
  values.set_temperature(_initial_temperature);
  values.set_ionic_fraction(ION_H_n, 1.e-6);
  values.set_ionic_fraction(ION_He_n, 1.e-6);

  return values;
}
