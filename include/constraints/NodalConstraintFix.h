
//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

// MOOSE includes
#include "Constraint.h"
#include "NodalConstraint.h"
#include "NeighborCoupleableMooseVariableDependencyIntermediateInterface.h"

class NodalConstraintFix : public NodalConstraint
{
public:
  static InputParameters validParams();

  NodalConstraintFix(const InputParameters & parameters);

  /**
   * Computes the nodal residual.
   */
  virtual void computeResidual(NumericVector<Number> & residual) override;

  /**
   * Computes the jacobian for the current element.
   */
  virtual void computeJacobian(SparseMatrix<Number> & jacobian) override;

protected:
  /**
   * This is the virtual that derived classes should override for computing the residual on
   * neighboring element.
   */
  virtual Real computeQpResidual(Moose::ConstraintType type) = 0;

  /**
   * This is the virtual that derived classes should override for computing the Jacobian on
   * neighboring element.
   */
  virtual Real computeQpJacobian(Moose::ConstraintJacobianType type) = 0;
};
