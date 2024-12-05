//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "DensityUpdateUnconstrainedMMA.h"
#include "vector.h"
#include <algorithm>
#include "MooseVariableScalar.h"

registerMooseObject("OptimizationApp", DensityUpdateUnconstrainedMMA);

InputParameters
DensityUpdateUnconstrainedMMA::validParams()
{
  InputParameters params = DensityUpdateMMA::validParams();
  params.addClassDescription("Computes updated densities based on objective function and "
                             "constraint sensitivities using MMA.");
  return params;
}

DensityUpdateUnconstrainedMMA::DensityUpdateUnconstrainedMMA(const InputParameters & parameters)
  : DensityUpdateMMA(parameters)
{
}

// void
// DensityUpdateUnconstrainedMMA::gatherElementData()
// {
//   TIME_SECTION("gatherElementData", 3, "Gathering Element Data");
//   _elem_data_map.clear();

//   for (const auto & sub_id : blockIDs())
//     for (const auto & elem : _mesh.getMesh().active_local_subdomain_elements_ptr_range(sub_id))
//     {
//       dof_id_type elem_id = elem->id();

//       ElementData data;

//       if (_objective_type == ObjectiveType::MIN)
//         data = ElementData(
//             dynamic_cast<MooseVariableFE<Real> *>(_design_density)->getElementalValue(elem),
//             dynamic_cast<MooseVariableFE<Real> *>(_old_design_density1)->getElementalValue(elem),
//             dynamic_cast<MooseVariableFE<Real> *>(_old_design_density2)->getElementalValue(elem),
//             dynamic_cast<const MooseVariableFE<Real> *>(_objective_sensitivity)
//                 ->getElementalValue(elem),
//             {},
//             dynamic_cast<MooseVariableFE<Real> *>(_lower_asymptotes)->getElementalValue(elem),
//             dynamic_cast<MooseVariableFE<Real> *>(_upper_asymptotes)->getElementalValue(elem),
//             elem->volume(),
//             0,
//             0,
//             0);
//       else if (_objective_type == ObjectiveType::MAX)
//         data = ElementData(
//             dynamic_cast<MooseVariableFE<Real> *>(_design_density)->getElementalValue(elem),
//             dynamic_cast<MooseVariableFE<Real> *>(_old_design_density1)->getElementalValue(elem),
//             dynamic_cast<MooseVariableFE<Real> *>(_old_design_density2)->getElementalValue(elem),
//             -dynamic_cast<const MooseVariableFE<Real> *>(_objective_sensitivity)
//                  ->getElementalValue(elem),
//             {},
//             dynamic_cast<MooseVariableFE<Real> *>(_lower_asymptotes)->getElementalValue(elem),
//             dynamic_cast<MooseVariableFE<Real> *>(_upper_asymptotes)->getElementalValue(elem),
//             elem->volume(),
//             0,
//             0,
//             0);

//       _elem_data_map[elem_id] = data;
//     }
// }

void
DensityUpdateUnconstrainedMMA::performMmaLoop()
{
  TIME_SECTION("performMmaLoop", 3, "Building MMA Subproblem");
  unsigned int m = 0;
  unsigned int n = _n_el;

  // Vector variables of size n
  std::vector<Real> xmin(n), xmax(n, 1);
  std::vector<Real> xval, xold1, xold2, df0dx, low, upp;
  std::vector<std::pair<dof_id_type, Real>> comm_xval, comm_df0dx, comm_xold1, comm_xold2, comm_low,
      comm_upp;

  // Scalar constants
  Real a0 = 1;

  // Loop over all elements to populate the vectors
  for (auto && [id, elem_data] : _elem_data_map)
  {
    comm_xval.emplace_back(id, elem_data.current_design_density);
    comm_xold1.emplace_back(id, elem_data.old_design_density1);
    comm_xold2.emplace_back(id, elem_data.old_design_density2);
    /*f0val not needed*/
    comm_df0dx.emplace_back(id, elem_data.objective_sensitivity);
    comm_low.emplace_back(id, elem_data.lower);
    comm_upp.emplace_back(id, elem_data.upper);
  }

  /// MPI communication
  if (_app.n_processors() > 1)
  {
    _communicator.allgather(comm_xval, false);
    _communicator.allgather(comm_xold1, false);
    _communicator.allgather(comm_xold2, false);
    _communicator.allgather(comm_df0dx, false);
    _communicator.allgather(comm_low, false);
    _communicator.allgather(comm_upp, false);

    std::sort(comm_xval.begin(), comm_xval.end());
    std::sort(comm_xold1.begin(), comm_xold1.end());
    std::sort(comm_xold2.begin(), comm_xold2.end());
    std::sort(comm_df0dx.begin(), comm_df0dx.end());
    std::sort(comm_low.begin(), comm_low.end());
    std::sort(comm_upp.begin(), comm_upp.end());
  }

  std::transform(comm_xval.begin(),
                 comm_xval.end(),
                 std::back_inserter(xval),
                 [](const std::pair<dof_id_type, Real> & p) { return p.second; });
  std::transform(comm_xold1.begin(),
                 comm_xold1.end(),
                 std::back_inserter(xold1),
                 [](const std::pair<dof_id_type, Real> & p) { return p.second; });
  std::transform(comm_xold2.begin(),
                 comm_xold2.end(),
                 std::back_inserter(xold2),
                 [](const std::pair<dof_id_type, Real> & p) { return p.second; });
  std::transform(comm_df0dx.begin(),
                 comm_df0dx.end(),
                 std::back_inserter(df0dx),
                 [](const std::pair<dof_id_type, Real> & p) { return p.second; });
  std::transform(comm_low.begin(),
                 comm_low.end(),
                 std::back_inserter(low),
                 [](const std::pair<dof_id_type, Real> & p) { return p.second; });
  std::transform(comm_upp.begin(),
                 comm_upp.end(),
                 std::back_inserter(upp),
                 [](const std::pair<dof_id_type, Real> & p) { return p.second; });

  /// MMA
  // NOTE: could be parametrized if needed
  Real epsimin = 0.0000001;
  Real raa0 = 0.00001;
  Real move = _move_limit;
  Real albefa = 0.1;
  Real asyinit = 0.5;
  Real asyincr = 1.2;
  Real asydecr = 0.7;
  std::vector<Real> eeen(n, 1);
  std::vector<Real> eeem(m, 1);

  // Calculation of the asymptotes low and upp
  std::vector<Real> zzz(n, 0);
  if (_t_step <= 2)
  {
    for (unsigned int i = 0; i < n; i++)
    {
      low[i] = xval[i] - asyinit * (xmax[i] - xmin[i]);
      upp[i] = xval[i] + asyinit * (xmax[i] - xmin[i]);
    }
  }
  else
  {
    for (unsigned int i = 0; i < n; i++)
    {
      zzz[i] = (xval[i] - xold1[i]) * (xold1[i] - xold2[i]);
      Real factor;
      if (zzz[i] > 0)
        factor = asyincr;
      else
        factor = asydecr;
      low[i] = xval[i] - factor * (xold1[i] - low[i]);
      upp[i] = xval[i] + factor * (upp[i] - xold1[i]);
      Real lowmin, lowmax, uppmin, uppmax;
      lowmin = xval[i] - 10 * (xmax[i] - xmin[i]);
      lowmax = xval[i] - 0.01 * (xmax[i] - xmin[i]);
      uppmin = xval[i] + 0.01 * (xmax[i] - xmin[i]);
      uppmax = xval[i] + 10 * (xmax[i] - xmin[i]);
      low[i] = std::max(low[i], lowmin);
      low[i] = std::min(low[i], lowmax);
      upp[i] = std::min(upp[i], uppmax);
      upp[i] = std::max(upp[i], uppmin);
    }
  }

  // Calculation of the bounds alpha and beta
  std::vector<Real> alpha(n, 0), beta(n, 0);
  for (unsigned int i = 0; i < n; i++)
  {
    Real zzz1, zzz2;
    zzz1 = low[i] + albefa * (xval[i] - low[i]);
    zzz2 = xval[i] - move * (xmax[i] - xmin[i]);
    zzz[i] = std::max(zzz1, zzz2);
    alpha[i] = std::max(zzz[i], xmin[i]);
    zzz1 = upp[i] - albefa * (upp[i] - xval[i]);
    zzz2 = xval[i] + move * (xmax[i] - xmin[i]);
    zzz[i] = std::min(zzz1, zzz2);
    beta[i] = std::min(zzz[i], xmax[i]);
  }

  // Calculations of p0, q0, P, Q and b
  std::vector<Real> xmami(n), xmamieps(n), xmamiinv(n), ux1(n), ux2(n), xl1(n), xl2(n), uxinv(n),
      xlinv(n), p0(n), q0(n), pq0(n);
  for (unsigned int i = 0; i < n; i++)
  {
    xmami[i] = xmax[i] - xmin[i];
    xmamieps[i] = 0.00001 * eeen[i];
    xmami[i] = std::max(xmami[i], xmamieps[i]);
    xmamiinv[i] = eeen[i] / xmami[i];
    ux1[i] = upp[i] - xval[i];
    ux2[i] = ux1[i] * ux1[i];
    xl1[i] = xval[i] - low[i];
    xl2[i] = xl1[i] * xl1[i];
    uxinv[i] = eeen[i] / ux1[i];
    xlinv[i] = eeen[i] / xl1[i];

    p0[i] = std::max(df0dx[i], 0.0);
    q0[i] = std::max(-df0dx[i], 0.0);
    pq0[i] = 0.001 * (p0[i] + q0[i]) + raa0 * xmamiinv[i];
    p0[i] += pq0[i];
    q0[i] += pq0[i];
    p0[i] *= ux2[i];
    q0[i] *= xl2[i];
  }

  // Solving the subproblem
  std::vector<Real> B(n), new_density(n);
  for (unsigned int i = 0; i < n; i++)
  {
    B[i] = (low[i] * p0[i] - upp[i] * q0[i] + (upp[i] - low[i]) * std::sqrt(p0[i] * q0[i])) /
           (p0[i] - q0[i]);
    new_density[i] = std::max(alpha[i], std::min(beta[i], B[i]));
  }

  for (auto && [id, elem_data] : _elem_data_map)
  {
    // Update the element data values
    elem_data.new_design_density = std::max(_x_min, new_density[id]);
    elem_data.new_lower = low[id];
    elem_data.new_upper = upp[id];
  }
}
