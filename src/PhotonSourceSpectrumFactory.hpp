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
 * @file PhotonSourceSpectrumFactory.hpp
 *
 * @brief Factory for PhotonSourceSpectrum instances.
 *
 * @author Bert Vandenbroucke (bv7@st-andrews.ac.uk)
 */
#ifndef PHOTONSOURCESPECTRUMFACTORY_HPP
#define PHOTONSOURCESPECTRUMFACTORY_HPP

#include "Log.hpp"
#include "ParameterFile.hpp"
#include "PhotonSourceSpectrum.hpp"

// implementations
#include "FaucherGiguerePhotonSourceSpectrum.hpp"
#include "MonochromaticPhotonSourceSpectrum.hpp"
#include "PlanckPhotonSourceSpectrum.hpp"
#include "WMBasicPhotonSourceSpectrum.hpp"

/**
 * @brief Factory for PhotonSourceSpectrum instances.
 */
class PhotonSourceSpectrumFactory {
public:
  /**
   * @brief Generate a PhotonSourceSpectrum based on the type chosen in the
   * parameter file (corresponding to the given role).
   *
   * @param role Role the PhotonSourceSpectrum will assume. Parameters will be
   * read from the corresponding parameter file block.
   * @param params ParameterFile to read from.
   * @param log Log to write logging info to.
   * @return Pointer to a newly created PhotonSourceSpectrum instance. Memory
   * management for the pointer needs to be done by the calling routine.
   */
  inline static PhotonSourceSpectrum *
  generate(std::string role, ParameterFile &params, Log *log = nullptr) {
    std::string type = params.get_value< std::string >(role + ":type", "None");
    if (log) {
      log->write_info("Requested PhotonSourceSpectrum for ", role, ": ", type);
    }
    if (type == "FaucherGiguere") {
      return new FaucherGiguerePhotonSourceSpectrum(role, params, log);
    } else if (type == "Monochromatic") {
      return new MonochromaticPhotonSourceSpectrum(role, params, log);
    } else if (type == "Planck") {
      return new PlanckPhotonSourceSpectrum(role, params, log);
    } else if (type == "WMBasic") {
      return new WMBasicPhotonSourceSpectrum(role, params, log);
    } else if (type == "None") {
      return nullptr;
    } else {
      cmac_error("Unknown PhotonSourceSpectrum type: \"%s\".", type.c_str());
      return nullptr;
    }
  }
};

#endif // PHOTONSOURCESPECTRUMFACTORY_HPP
