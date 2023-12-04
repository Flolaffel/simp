# geht nicht mit Master Acion
# keine Mehrwert zu normalen StressDivergenceKernels

nx = 2
ny = 2

[GlobalParams]
  displacements = 'disp_x disp_y'
  large_kinematics = false
  stabilize_strain = false
[]

[Variables]
  [disp_x]
  []
  [disp_y]
  []
  [strain_zz]
  []
[]

[Mesh]
  [MeshGenerator]
    type = GeneratedMeshGenerator
    dim = 2
    nx = ${nx}
    ny = ${ny}
    xmin = 0
    xmax = ${nx}
    ymin = 0
    ymax = ${ny}
  []
  [node]
    type = ExtraNodesetGenerator
    input = MeshGenerator
    new_boundary = pull
    coord = '2 0 0'
  []
  [push]
    type = ExtraNodesetGenerator
    input = node
    new_boundary = push
    coord = '0 2 0'
  []
  [sidesets]
    type = SideSetsFromNodeSetsGenerator
    input = push
  []
[]

[Kernels]
  [sdx]
    type = TotalLagrangianStressDivergence
    variable = disp_x
    out_of_plane_strain = strain_zz
    component = 0
  []
  [sdy]
    type = TotalLagrangianStressDivergence
    variable = disp_y
    out_of_plane_strain = strain_zz
    component = 1
  []
  [wps]
    type = TotalLagrangianWeakPlaneStress
    variable = strain_zz
  []
[]

[AuxVariables]
  [aux_strain_zz]
    order = CONSTANT
    family = MONOMIAL
  []
[]

[AuxKernels]
  [strain_zz]
    type = RankTwoAux
    rank_two_tensor = total_strain
    variable = aux_strain_zz
    index_i = 2
    index_j = 2
  []
[]

[BCs]
  [no_x]
    type = DirichletBC
    variable = disp_y
    boundary = pull
    value = 0.0
  []
  [no_y]
    type = DirichletBC
    variable = disp_x
    boundary = left
    value = 0.0
  []
[]

[NodalKernels]
  [pull]
    type = ConstantRate
    variable = disp_y
    boundary = push
    rate = -1
  []
[]

[Materials]
  [elasticity_tensor]
    type = ComputeIsotropicElasticityTensor
    youngs_modulus = 0.1250000009
    poissons_ratio = 0.3
  []
  [strain]
    type = ComputeLagrangianWPSStrain
    out_of_plane_strain = strain_zz
  []
  [stress]
    type = ComputeLagrangianLinearElasticStress
  []
[]

[Preconditioning]
  [smp]
    type = SMP
    full = true
  []
[]

[Executioner]
  type = Steady
  solve_type = NEWTON
  petsc_options_iname = '-pc_type -pc_factor_mat_solver_package'
  petsc_options_value = 'lu superlu_dist'
  nl_abs_tol = 1e-8
[]

[Outputs]
  exodus = true
[]

[Debug]
  #  show_material_props = true
  #show_execution_order = ALWAYS
[]
