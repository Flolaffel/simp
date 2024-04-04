//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "Filter.h"
#include "MooseTypes.h"

/**
 * Element user object that filters the objective function sensitivities via a radial average user
 * objects. This object can be used to apply a Solid Isotropic Material Penalization (SIMP) to
 * optimization.
 */
class SensitivityFilterCustom : public Filter
{
public:
  static InputParameters validParams();

  SensitivityFilterCustom(const InputParameters & parameters);

  virtual void initialize() override;
  virtual void execute() override;
  virtual void finalize() override{};
  virtual void threadJoin(const UserObject & y) override;

protected:
  /// Number of variables
  unsigned int _n_vars;
  /// Variable vector
  std::vector<MooseWritableVariable *> _sensitivities;
  /// Pseudo-density variable name
  VariableName _design_density_name;
  /// The pseudo-density variable
  MooseVariable * _design_density;

private:
  struct ElementData
  {
    std::vector<Real> sensitivities;
    Real design_density;
    std::vector<Real> filtered_sensitivities;
    ElementData() = default;
    ElementData(std::vector<Real> sens, Real dens, std::vector<Real> filt_sens)
      : sensitivities(sens), design_density(dens), filtered_sensitivities(filt_sens)
    {
    }
  };

  /// Data structure to hold old density, sensitivity, volume, current density.
  std::map<dof_id_type, ElementData> _elem_data_map;

  /**
   * Gathers element date necessary to perform the bisection algorithm for optimization
   */
  void gatherElementData();

  /**
   * Filters the compliance sensitivity in respect to the density
   */
  void updateSensitivitiesSensitivityFilter();

  /**
   * Filters the compliance and volume sensitivities in respect to the density
   */
  void updateSensitivitiesDensityFilter();
};
