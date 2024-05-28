//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "FilterBase.h"
#include "MooseTypes.h"
#include "GatherElementData.h"

/**
 * Element user object that filters the objective function (and constraint) sensitivities, depending
 * on filter type.
 */

class SensitivityFilterCustom : public FilterBase
{
public:
  static InputParameters validParams();

  SensitivityFilterCustom(const InputParameters & parameters);

  virtual void initialize() override;
  virtual void execute() override;
  virtual void threadJoin(const UserObject & y) override;

protected:
  /// Number of variables
  unsigned int _n_vars;
  /// Variable vector
  std::vector<MooseWritableVariable *> _sensitivities;
  /// Design density variable name
  VariableName _design_density_name;
  /// The design density variable
  MooseVariable * _design_density;
  /// Filtered density variable name
  VariableName _filtered_density_name;
  /// Filtered density variable
  MooseVariable * _filtered_density;

private:
  /// Data structures to hold element data
  std::vector<std::vector<std::pair<dof_id_type, Real>>> _sensitivities_vec;
  std::vector<std::pair<dof_id_type, Real>> _design_density_vec;
  std::vector<std::pair<dof_id_type, Real>> _filtered_density_vec;
  std::map<dof_id_type, std::vector<Real>> _filtered_sensitivities_map;

  /**
   * Gathers element data
   */
  void gatherElementData();

  /**
   * Filters the objective function sensitivity
   */
  void updateSensitivitiesSensitivityFilter();

  /**
   * Updates the sensitivities for density filter usage
   */
  void updateSensitivitiesDensityFilter();

  /**
   * Updates the sensitivities for heaviside projection usage
   */
  void updateSensitivitiesHeaviside();
};
