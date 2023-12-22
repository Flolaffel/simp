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
  virtual void threadJoin(const UserObject &) override{};

protected:
  /// Sensitivity with respect to density
  MooseWritableVariable * _compliance_sensitivity;
  /// Pseudo-density variable name
  VariableName _design_density_name;
  /// The pseudo-density variable
  MooseVariable * _design_density;
  /// Sensitivity with respect to density
  MooseWritableVariable * _volume_sensitivity;

private:
  struct ElementData
  {
    Real compliance_sensitivity;
    Real design_density;
    Real volume_sensitivity;
    Real new_compliance_sensitivity;
    Real new_volume_sensitivity;
    ElementData() = default;
    ElementData(Real dc, Real dens, Real dv, Real filt_dc, Real filt_dv)
      : compliance_sensitivity(dc),
        design_density(dens),
        volume_sensitivity(dv),
        new_compliance_sensitivity(filt_dc),
        new_volume_sensitivity(filt_dv)
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
