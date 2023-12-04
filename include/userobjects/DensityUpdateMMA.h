//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "ElementUserObject.h"
#include "MooseTypes.h"
#include "TransientInterface.h"

/**
 * Element user object that performs SIMP optimization using a bisection algorithm using a volume
 * constraint.
 * ONLY USE AT timestep_end TO EXECUTE AFTER SensitivityFilter
 */
class DensityUpdateMMA : public ElementUserObject
{
public:
  static InputParameters validParams();

  DensityUpdateMMA(const InputParameters & parameters);

  virtual void initialize() override;
  virtual void timestepSetup() override{};
  virtual void execute() override;
  virtual void finalize() override{};
  virtual void threadJoin(const UserObject &) override{};

protected:
  /// The system mesh
  const MooseMesh & _mesh;
  /// The name of the pseudo-density variable
  const VariableName _design_density_name;
  /// The name of the old pseudo-density variable
  const VariableName _old_design_density_name;
  /// The elasticity compliance sensitivity name
  const VariableName _density_sensitivity_name;
  /// The pseudo-density variable
  MooseWritableVariable * _design_density;
  /// The old pseudo-density variable
  MooseWritableVariable * _old_design_density1;
  /// The older pseudo-density variable
  MooseWritableVariable * _old_design_density2;
  /// The filtered density sensitivity variable
  const MooseWritableVariable * _density_sensitivity;
  /// The volume fraction to be enforced
  const Real _volume_fraction;
  /// Column vector with the lower asymptotes from the previous iteration (provided that iter>1)
  MooseWritableVariable * _lower_asymptotes;
  /// Column vector with the upper asymptotes from the previous iteration (provided that iter>1)
  MooseWritableVariable * _upper_asymptotes;

private:
  struct ElementData
  {
    Real current_density;
    Real old_density1;
    Real old_density2;
    Real sensitivity;
    Real volume;
    Real lower;
    Real upper;
    Real new_density;
    Real new_lower;
    Real new_upper;
    ElementData() = default;
    ElementData(Real curr_dens,
                Real old_dens1,
                Real old_dens2,
                Real sens,
                Real vol,
                Real low,
                Real upp,
                Real filt_dens,
                Real new_low,
                Real new_upp)
      : current_density(curr_dens),
        old_density1(old_dens1),
        old_density2(old_dens2),
        sensitivity(sens),
        volume(vol),
        lower(low),
        upper(upp),
        new_density(filt_dens),
        new_lower(new_low),
        new_upper(new_upp)
    {
    }
  };

  /**
   * Gathers element date necessary to perform the bisection algorithm for optimization
   */
  void gatherElementData();

  /**
   * Performs the optimality criterion loop (bisection)
   */
  void performMMALoop();

  /// Total volume allowed for volume contraint
  Real _total_allowable_volume;

  /// Data structure to hold old density, sensitivity, volume, current density.
  std::map<dof_id_type, ElementData> _elem_data_map;

  std::vector<Real> MMASubSolve(Real m,
                                Real n,
                                Real epsimin,
                                std::vector<Real> low,
                                std::vector<Real> upp,
                                std::vector<Real> alpha,
                                std::vector<Real> beta,
                                std::vector<Real> p0,
                                std::vector<Real> q0,
                                std::vector<Real> P,
                                std::vector<Real> Q,
                                Real a0,
                                Real a,
                                Real b,
                                Real c,
                                Real d);

  std::vector<Real> AbsVec(std::vector<Real> vector);
  Real NormVec(std::vector<Real> vector);
};
