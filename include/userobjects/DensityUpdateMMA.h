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

/**
 * Element user object that performs SIMP optimization using the Method of Moving Asymptotes (MMA)
 * Use after Design Responses and Sensitivity Filter
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
  /// The pseudo-density variable
  MooseWritableVariable * _design_density;
  /// The old pseudo-density variable
  MooseWritableVariable * _old_design_density1;
  /// The older pseudo-density variable
  MooseWritableVariable * _old_design_density2;
  /// The objective function sensitivity name
  const VariableName _objective_sensitivity_name;
  /// The objective function sensitivity variable
  const MooseVariable * _objective_sensitivity;
  /// Number of constraints
  unsigned int _n_cons;
  /// The constraint value variable names
  const std::vector<VariableName> _constraint_value_names;
  /// The constraint values
  std::vector<MooseVariableScalar *> _constraint_values;
  /// The constraint sensitivity variable names
  const std::vector<VariableName> _constraint_sensitivity_names;
  /// The constraint sensitivities
  std::vector<MooseVariable *> _constraint_sensitivities;
  /// Column vector with the lower asymptotes from the previous iteration (provided that iter>1)
  MooseWritableVariable * _lower_asymptotes;
  /// Column vector with the upper asymptotes from the previous iteration (provided that iter>1)
  MooseWritableVariable * _upper_asymptotes;

private:
  struct ElementData
  {
    Real current_design_density;
    Real old_design_density1;
    Real old_design_density2;
    Real objective_sensitivity;
    std::vector<Real> constraint_sensitivities;
    Real lower;
    Real upper;
    Real volume;
    Real new_design_density;
    Real new_lower;
    Real new_upper;
    ElementData() = default;
    ElementData(Real curr_d_dens,
                Real old_dens1,
                Real old_dens2,
                Real oc,
                std::vector<Real> dgs,
                Real low,
                Real upp,
                Real vol,
                Real new_dens,
                Real new_low,
                Real new_upp)
      : current_design_density(curr_d_dens),
        old_design_density1(old_dens1),
        old_design_density2(old_dens2),
        objective_sensitivity(oc),
        constraint_sensitivities(dgs),
        lower(low),
        upper(upp),
        volume(vol),
        new_design_density(new_dens),
        new_lower(new_low),
        new_upper(new_upp)
    {
    }
  };

  /**
   * Gathers element data
   */
  void gatherElementData();

  /// Data structure to hold old element data
  std::map<dof_id_type, ElementData> _elem_data_map;

  /// Number of elements
  unsigned int _n_el;

  /// Move limit
  Real _move_limit;

  /**
   * Performs the MMA setup
   */
  void performMmaLoop();

  /**
   * Solves MMA subproblem with primal-dual Newton method
   */
  std::vector<Real> MmaSubSolve(unsigned int m,
                                unsigned int n,
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
};
