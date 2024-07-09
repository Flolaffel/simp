//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "NodalConstraintFix.h"

// MOOSE includes
#include "Assembly.h"
#include "MooseVariableFE.h"
#include "SystemBase.h"

#include "libmesh/sparse_matrix.h"

#include "libmesh/petsc_matrix.h"
#include "libmesh/petsc_vector.h"
#include "petscmat.h"

InputParameters
NodalConstraintFix::validParams()
{
  InputParameters params = NodalConstraint::validParams();
  return params;
}

NodalConstraintFix::NodalConstraintFix(const InputParameters & parameters)
  : NodalConstraint(parameters)
{
}

void
NodalConstraintFix::computeResidual(NumericVector<Number> & residual)
{
  if ((_weights.size() == 0) && (_primary_node_vector.size() == 1))
    _weights.push_back(1.0);

  std::vector<dof_id_type> primarydof = _var.dofIndices();
  std::vector<dof_id_type> secondarydof = _var_secondary.dofIndicesNeighbor();

  DenseVector<Number> re(primarydof.size());
  DenseVector<Number> neighbor_re(secondarydof.size());

  re.zero();
  neighbor_re.zero();

  for (_i = 0; _i < secondarydof.size(); ++_i)
  {
    for (_j = 0; _j < primarydof.size(); ++_j)
    {
      switch (_formulation)
      {
        case Moose::Penalty:
          re(_j) += computeQpResidual(Moose::Primary) * _var.scalingFactor();
          neighbor_re(_i) += computeQpResidual(Moose::Secondary) * _var_secondary.scalingFactor();
          break;
        case Moose::Kinematic:
          // Transfer the current residual of the secondary node to the primary nodes
          Real res = residual(secondarydof[_i]);
          re(_j) += res * _weights[_j];
          neighbor_re(_i) +=
              -res / _primary_node_vector.size() + computeQpResidual(Moose::Secondary);
          break;
      }
    }
  }
  // We've already applied scaling
  if (!primarydof.empty())
    addResiduals(_assembly, re, primarydof, /*scaling_factor=*/1);
  if (!secondarydof.empty())
    addResiduals(_assembly, neighbor_re, secondarydof, /*scaling_factor=*/1);
}

void
NodalConstraintFix::computeJacobian(SparseMatrix<Number> & jacobian)
{
  if ((_weights.size() == 0) && (_primary_node_vector.size() == 1))
    _weights.push_back(1.0);

  // Calculate the dense-block Jacobian entries
  std::vector<dof_id_type> secondarydof = _var_secondary.dofIndicesNeighbor();
  std::vector<dof_id_type> primarydof = _var.dofIndices();
  dof_id_type n_dofs = _sys.system().n_dofs();
  std::vector<dof_id_type> alldof(n_dofs);
  std::iota(std::begin(alldof), std::end(alldof), 0);

  DenseMatrix<Number> Kee(primarydof.size(), primarydof.size());
  DenseMatrix<Number> Ken(primarydof.size(), secondarydof.size());
  DenseMatrix<Number> Kne(secondarydof.size(), primarydof.size());
  // DenseMatrix<Number> Kzero(secondarydof.size(), alldof.size());

  Kee.zero();
  Ken.zero();
  Kne.zero();
  // Kzero.zero();

  // DenseMatrix<Number> Ktransfer(primarydof.size(), alldof.size());
  // Ktransfer.zero();

  // std::vector<dof_id_type> indices;
  // std::vector<Real> values;

  // std::cout << "TEST" << std::endl;

  // TODO: Extend to work with multiple secondary nodes
  for (_i = 0; _i < secondarydof.size(); ++_i)
  {
    // if (Moose::Kinematic)
    // {
    //   jacobian.get_row(secondarydof[_i], indices, values);
    //   // for (auto & _j : indices)
    //   // {
    //   //   auto index = &_j - &indices[0];
    //   //   Kzero(_i, _j) -= values[index];
    //   // }
    // }
    // if (Moose::Kinematic)
    // {
    //   for (_j = 0; _j < alldof.size(); ++_j)
    //   {
    //     Kzero(_i, _j) += -jacobian(secondarydof[_i], _j);
    //   }
    // }
    for (_j = 0; _j < primarydof.size(); ++_j)
    {
      switch (_formulation)
      {
        case Moose::Penalty:
          Kee(_j, _j) += computeQpJacobian(Moose::PrimaryPrimary);
          Ken(_j, _i) += computeQpJacobian(Moose::PrimarySecondary);
          Kne(_i, _j) += computeQpJacobian(Moose::SecondaryPrimary);
          break;
        case Moose::Kinematic:
          Kee(_j, _j) = 0.;
          Ken(_j, _i) += jacobian(secondarydof[_i], primarydof[_j]) * _weights[_j];
          // Kne(_i, _j) += -jacobian(secondarydof[_i], primarydof[_j]) / primarydof.size() +
          //                computeQpJacobian(Moose::SecondaryPrimary);
          // Kne(_i, _j) += -jacobian(secondarydof[_i], primarydof[_j]) +
          //                computeQpJacobian(Moose::SecondaryPrimary);
          Kne(_i, _j) += computeQpJacobian(Moose::SecondaryPrimary);
          // for (unsigned int k = 0; k < alldof.size(); ++k)
          // {
          //   Ktransfer(_j, k) += jacobian(secondarydof[_i], k);
          //   if (jacobian(secondarydof[_i], k) != 0)
          //   {
          //     // transfer_rows.push_back(primarydof[_j]);
          //     // transfer_cols.push_back(k);
          //     // transfer_values.push_back(jacobian(secondarydof[_i], k));
          //     nonzerodof.push_back(k);
          //   }
          // }
          break;
      }
    }
  }

  // std::cout << indices.size() << std::endl;

  // std::vector<dof_id_type> nonzero_dofs;
  // std::vector<Real> nonzero_values;

  // for (int i = 0; i < values.size(); i++)
  // {
  //   if (values[i] != 0)
  //   {
  //     nonzero_values.push_back(values[i]);
  //     nonzero_dofs.push_back(indices[i]);
  //   }
  // }

  // DenseMatrix<Number> Ktransfer_test(primarydof.size(), nonzero_dofs.size());
  // DenseMatrix<Number> Kzero_test(secondarydof.size(), nonzero_dofs.size());
  // Ktransfer_test.zero();
  // Kzero_test.zero();

  // for (int i = 0; i < primarydof.size(); i++)
  //   for (int j = 0; j < nonzero_dofs.size(); j++)
  //   {
  //     Ktransfer_test(i, j) = nonzero_values[j];
  //   }

  // for (int i = 0; i < secondarydof.size(); i++)
  //   for (int j = 0; j < nonzero_dofs.size(); j++)
  //   {
  //     Kzero_test(i, j) = -nonzero_values[j];
  //   }

  // std::cout << "Kee: " << std::endl;
  // Kee.print();
  // std::cout << std::endl << "Ken: " << std::endl;
  // Ken.print();
  // std::cout << std::endl << "Kne: " << std::endl;
  // Kne.print();
  // std::cout << std::endl << "Kzero: " << std::endl;
  // Kzero.print();
  // std::cout << std::endl << "Kzero_test: " << std::endl;
  // Kzero_test.print();
  // std::cout << std::endl << "Ktransfer: " << std::endl;
  // Ktransfer.print();
  // std::cout << std::endl << "Ktransfer_test: " << std::endl;
  // Ktransfer_test.print();
  // std::cout << std::endl;
  // for (auto & out : nonzerodof)
  //   std::cout << out << ", ";
  // std::cout << std::endl;

  // std::cout << "TEST1" << std::endl;

  // std::cout << jacobian.m() << " | " << jacobian.n() << " | " << jacobian.local_m() << " | "
  //           << jacobian.local_n() << std::endl;

  // LIBMESH_CHKERR(MatZeroRows(petsc_jacobian->mat(),
  //                            cast_int<PetscInt>(secondarydof.size()),
  //                            numeric_petsc_cast(secondarydof.data()),
  //                            0,
  //                            NULL,
  //                            NULL));

  // petsc_jacobian.zero_rows(secondarydof);

  // std::cout << "TEST2" << std::endl;

  addJacobian(_assembly, Kee, primarydof, primarydof, _var.scalingFactor());
  // addJacobian(_assembly, Ken, primarydof, secondarydof, _var.scalingFactor());
  addJacobian(_assembly, Kne, secondarydof, primarydof, _var_secondary.scalingFactor());
  // addJacobian(_assembly, Kzero, secondarydof, alldof, _var_secondary.scalingFactor());
  // addJacobian(_assembly, Kzero_test, secondarydof, nonzero_dofs, _var_secondary.scalingFactor());
  // // addJacobian(_assembly, Ktransfer, primarydof, alldof, _var_secondary.scalingFactor());
  // addJacobian(_assembly, Ktransfer_test, primarydof, nonzero_dofs,
  // _var_secondary.scalingFactor());

  // std::cout << "TEST3" << std::endl;

  // Calculate and cache the diagonal secondary-secondary entries
  for (_i = 0; _i < secondarydof.size(); ++_i)
  {
    Number value = 0.0;
    switch (_formulation)
    {
      case Moose::Penalty:
        value = computeQpJacobian(Moose::SecondarySecondary) * primarydof.size();
        break;
      case Moose::Kinematic:
        // value = -jacobian(secondarydof[_i], secondarydof[_i]) / primarydof.size() +
        //         computeQpJacobian(Moose::SecondarySecondary);
        // value = -jacobian(secondarydof[_i], secondarydof[_i]) +
        //         computeQpJacobian(Moose::SecondarySecondary) * primarydof.size();
        value = computeQpJacobian(Moose::SecondarySecondary) * primarydof.size();
        break;
    }
    addJacobianElement(
        _assembly, value, secondarydof[_i], secondarydof[_i], _var_secondary.scalingFactor());
  }

  std::vector<dof_id_type> indices;
  std::vector<Real> values;

  // std::cout << "TEST" << std::endl;

  // TODO: Extend to work with multiple secondary nodes
  if (Moose::Kinematic)
    for (_i = 0; _i < secondarydof.size(); ++_i)
    {
      std::vector<dof_id_type> indices;
      std::vector<Real> values;

      jacobian.get_row(secondarydof[_i], indices, values);

      std::vector<dof_id_type> nonzero_dofs;
      std::vector<Real> nonzero_values;

      for (int i = 0; i < values.size(); i++)
      {
        if (values[i] != 0)
        {
          nonzero_values.push_back(values[i]);
          nonzero_dofs.push_back(indices[i]);
        }
      }

      DenseMatrix<Number> Ktransfer_test(primarydof.size(), nonzero_dofs.size());
      DenseMatrix<Number> Kzero_test(secondarydof.size(), nonzero_dofs.size());
      Ktransfer_test.zero();
      Kzero_test.zero();

      for (int i = 0; i < primarydof.size(); i++)
        for (int j = 0; j < nonzero_dofs.size(); j++)
        {
          Ktransfer_test(i, j) = nonzero_values[j];
        }

      for (int i = 0; i < secondarydof.size(); i++)
        for (int j = 0; j < nonzero_dofs.size(); j++)
        {
          Kzero_test(i, j) = -nonzero_values[j];
        }

      addJacobian(
          _assembly, Kzero_test, secondarydof, nonzero_dofs, _var_secondary.scalingFactor());
      addJacobian(
          _assembly, Ktransfer_test, primarydof, nonzero_dofs, _var_secondary.scalingFactor());
    }

  // std::cout << "TEST4" << std::endl;
}
