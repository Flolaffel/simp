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
 * Element user object that performs SIMP optimization using a bisection algorithm using a volume
 * constraint.
 * ONLY USE AT timestep_end TO EXECUTE AFTER SensitivityFilter
 */

enum class UpdateScheme
{
  OC,
  MMA
};

class DensityUpdateCustom : public Filter
{
public:
  static InputParameters validParams();

  DensityUpdateCustom(const InputParameters & parameters);

  virtual void initialize() override;
  virtual void timestepSetup() override{};
  virtual void execute() override;
  virtual void finalize() override{};
  virtual void threadJoin(const UserObject &) override{};

protected:
  /// The update scheme
  const UpdateScheme _update_scheme;
  /// The pseudo-density variable
  MooseWritableVariable * _design_density;
  /// The pseudo-density variable
  MooseWritableVariable * _physical_density;
  /// The compliance sensitivity name
  const VariableName _compliance_sensitivity_name;
  /// The filtered compliance sensitivity variable
  const MooseWritableVariable * _compliance_sensitivity;
  /// The volume sensitivity name
  const VariableName _volume_sensitivity_name;
  /// The filtered volume sensitivity variable
  const MooseWritableVariable * _volume_sensitivity;
  /// The volume fraction to be enforced
  const Real _volume_fraction;

  /**
   * Get update scheme
   * @return enum
   */
  static MooseEnum getUpdateSchemeEnum();

private:
  struct ElementData
  {
    Real current_density;
    Real compliance_sensitivity;
    Real volume_sensitivity;
    Real volume;
    Real new_design_density;
    Real new_phys_density;
    ElementData() = default;
    ElementData(Real dens, Real dc, Real dv, Real vol, Real new_dens, Real filt_dens)
      : current_density(dens),
        compliance_sensitivity(dc),
        volume_sensitivity(dv),
        volume(vol),
        new_design_density(new_dens),
        new_phys_density(filt_dens)
    {
    }
  };

  /**
   * Gathers element date necessary to perform the bisection algorithm for optimization
   */
  void gatherElementData();

  /**
   * Performs the opti<mality criterion loop (bisection)
   */
  void performOptimCritLoop();

  Real computeUpdatedDensity(Real current_density, Real dc, Real dv, Real lmid);

  /// Total volume allowed for volume contraint
  Real _total_allowable_volume;

  /// Data structure to hold old density, sensitivity, volume, current density.
  std::map<dof_id_type, ElementData> _elem_data_map;

  /// Lower bound for bisection algorithm
  Real _lower_bound;
  /// Upper bound for bisection algorithm
  Real _upper_bound;
};
