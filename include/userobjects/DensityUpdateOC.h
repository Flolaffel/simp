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

class DensityUpdateOC : public Filter
{
public:
  static InputParameters validParams();

  DensityUpdateOC(const InputParameters & parameters);

  virtual void initialize() override;
  virtual void timestepSetup() override{};
  virtual void execute() override;
  virtual void finalize() override{};
  virtual void threadJoin(const UserObject &) override{};

  /**
   * Get filter type
   * @return enum
   */
  static MooseEnum getFilterEnum();

protected:
  enum class FilterType
  {
    NONE,
    DENSITY,
    HEAVISIDE
  };

  /// The pseudo-density variable
  MooseWritableVariable * _design_density;
  /// The filtered density variable
  MooseWritableVariable * _filtered_density;
  /// The pseudo-density variable
  MooseWritableVariable * _physical_density;
  /// The objective function sensitivity name
  const VariableName _objective_sensitivity_name;
  /// The objective function sensitivity variable
  const MooseVariable * _objective_sensitivity;
  /// The constraint sensitivity variable names
  const VariableName _volume_sensitivity_name;
  /// The constraint sensitivities
  const MooseVariable * _volume_sensitivity;
  /// The volume fraction to be enforced
  Real _volume_fraction;
  /// Move limit
  const Real _move_limit;
  /// Lower bound for bisection algorithm
  const Real _lower_bound;
  /// Upper bound for bisection algorithm
  const Real _upper_bound;
  /// Whether to filter the densities
  const FilterType _filter_type;
  /// Heaviside parameter
  Real _beta_0;
  /// heaviside parameter
  Real _eta;

private:
  struct ElementData
  {
    Real current_design_density;
    Real objective_sensitivity;
    Real volume_sensitivity;
    Real volume;
    Real new_design_density;
    Real new_filt_density;
    Real new_proj_density;
    ElementData() = default;
    ElementData(
        Real curr_d_dens, Real oc, Real dv, Real vol, Real new_dens, Real filt_dens, Real proj_dens)
      : current_design_density(curr_d_dens),
        objective_sensitivity(oc),
        volume_sensitivity(dv),
        volume(vol),
        new_design_density(new_dens),
        new_filt_density(filt_dens),
        new_proj_density(proj_dens)
    {
    }
  };

  /**
   * Gathers element date necessary to perform the bisection algorithm for optimization
   */
  void gatherElementData();

  /// Total volume allowed for volume constraint
  Real _total_allowable_volume;

  /// Data structure to hold old density, sensitivity, volume, current density.
  std::map<dof_id_type, ElementData> _elem_data_map;

  /// Number of elements
  unsigned int _n_el;

  /**
   * Performs the optimality criterion loop (bisection)
   */
  void performOcLoop();

  Real computeUpdatedDensity(Real current_density, Real dc, Real dv, Real lmid);

  std::vector<Real> densityFilter(std::vector<Real> density);

  std::vector<Real> heavisideProjection(std::vector<Real> filtered_density);
};
