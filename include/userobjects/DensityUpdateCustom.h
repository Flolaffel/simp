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
  /// Flag for OC
  const bool _use_oc;
  /// Flag for MMA
  const bool _use_mma;
  /// The pseudo-density variable
  MooseWritableVariable * _design_density;
  /// The old pseudo-density variable
  MooseWritableVariable * _old_design_density1;
  /// The older pseudo-density variable
  MooseWritableVariable * _old_design_density2;
  /// The pseudo-density variable
  MooseWritableVariable * _physical_density;
  /// The compliance sensitivity name
  const VariableName _compliance_sensitivity_name;
  /// The filtered compliance sensitivity variable
  const MooseVariable * _compliance_sensitivity;
  /// The volume sensitivity name
  const VariableName _volume_sensitivity_name;
  /// The filtered volume sensitivity variable
  const MooseVariable * _volume_sensitivity;
  /// The volume fraction to be enforced
  const Real _volume_fraction;
  /// Column vector with the lower asymptotes from the previous iteration (provided that iter>1)
  MooseWritableVariable * _lower_asymptotes;
  /// Column vector with the upper asymptotes from the previous iteration (provided that iter>1)
  MooseWritableVariable * _upper_asymptotes;

  /**
   * Get update scheme
   * @return enum
   */
  static MooseEnum getUpdateSchemeEnum();

private:
  struct ElementData
  {
    Real current_design_density;
    Real current_physical_density;
    Real old_design_density1;
    Real old_design_density2;
    Real compliance_sensitivity;
    Real volume_sensitivity;
    Real lower;
    Real upper;
    Real volume;
    Real new_design_density;
    Real new_phys_density;
    Real new_lower;
    Real new_upper;
    ElementData() = default;
    ElementData(Real curr_d_dens,
                Real curr_p_dens,
                Real old_dens1,
                Real old_dens2,
                Real dc,
                Real dv,
                Real low,
                Real upp,
                Real vol,
                Real new_dens,
                Real filt_dens,
                Real new_low,
                Real new_upp)
      : current_design_density(curr_d_dens),
        current_physical_density(curr_p_dens),
        old_design_density1(old_dens1),
        old_design_density2(old_dens2),
        compliance_sensitivity(dc),
        volume_sensitivity(dv),
        lower(low),
        upper(upp),
        volume(vol),
        new_design_density(new_dens),
        new_phys_density(filt_dens),
        new_lower(new_low),
        new_upper(new_upp)
    {
    }
  };

  /**
   * Gathers element date necessary to perform the bisection algorithm for optimization
   */
  void gatherElementData();

  /// Total volume allowed for volume contraint
  Real _total_allowable_volume;

  /// Data structure to hold old density, sensitivity, volume, current density.
  std::map<dof_id_type, ElementData> _elem_data_map;

  /**
   * Performs the optimality criterion loop (bisection)
   */
  void performOcLoop();

  Real computeUpdatedDensity(Real current_density, Real dc, Real dv, Real lmid);

  /// Lower bound for bisection algorithm
  Real _lower_bound;
  /// Upper bound for bisection algorithm
  Real _upper_bound;

  /**
   * Performs the MMA setup
   */
  void performMmaLoop();

  std::vector<Real> MmaSubSolve(Real m,
                                Real n,
                                Real epsimin,
                                std::vector<Real> low,
                                std::vector<Real> upp,
                                std::vector<Real> alpha,
                                std::vector<Real> beta,
                                std::vector<Real> p0,
                                std::vector<Real> q0,
                                std::vector<std::vector<Real>> P,
                                std::vector<std::vector<Real>> Q,
                                Real a0,
                                std::vector<Real> a,
                                std::vector<Real> b,
                                std::vector<Real> c,
                                std::vector<Real> d);

  // Helper functions
  std::vector<Real> AbsVec(std::vector<Real> vector);
  Real NormVec(std::vector<Real> vector);
  std::vector<Real> DensityFilter(std::vector<Real> density);

  Real getDeterminant(std::vector<std::vector<Real>> vector);
  std::vector<std::vector<Real>> getTranspose(const std::vector<std::vector<Real>> matrix);
  std::vector<std::vector<Real>> getCofactor(const std::vector<std::vector<Real>> vector);
  std::vector<std::vector<Real>> getInverse(std::vector<std::vector<Real>> vector);
};
