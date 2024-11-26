nx = 150
ny = 75
xmax = 100
ymax = 50
l_el = '${fparse xmax/nx}'
E0 = 210000
nu = 0.3

[Problem]
  extra_tag_matrices = K
[]

[GlobalParams]
  displacements = 'disp_x disp_y'
[]

[Mesh]
  [MeshGenerator]
    type = GeneratedMeshGenerator
    dim = 2
    nx = ${nx}
    ny = ${ny}
    xmin = 0
    xmax = ${xmax}
    ymin = 0
    ymax = ${ymax}
  []
  [node]
    type = ExtraNodesetGenerator
    input = MeshGenerator
    new_boundary = pull
    coord = '${fparse xmax} 0 0; ${fparse xmax-l_el} 0 0; ${fparse xmax-2*l_el} 0 0; ${fparse xmax-3*l_el} 0 0; ${fparse xmax-4*l_el} 0 0; ${fparse xmax-5*l_el} 0 0'
  []
  [push]
    type = ExtraNodesetGenerator
    input = node
    new_boundary = push
    coord = '0 50 0'
  []
  [sidesets]
    type = SideSetsFromNodeSetsGenerator
    input = push
  []
[]

[Variables]
  [disp_x]
  []
  [disp_y]
  []
[]

[Kernels]
  [stress_x]
    type = StressDivergenceTensors
    component = 0
    variable = disp_x
    extra_matrix_tags = K
  []
  [stress_y]
    type = StressDivergenceTensors
    component = 1
    variable = disp_y
    extra_matrix_tags = K
  []
[]
[BCs]
  [no_x]
    type = DirichletBC
    variable = disp_y
    boundary = left
    value = 0.0
    matrix_tags = system
  []
  [no_y]
    type = DirichletBC
    variable = disp_x
    boundary = left
    value = 0.0
    matrix_tags = system
  []
  [pull]
    type = NeumannBC
    variable = disp_y
    boundary = pull
    value = -0.2
    matrix_tags = system
  []
[]

[Materials]
  [elasticity_tensor]
    type = ComputeIsotropicElasticityTensorPlaneStress
    youngs_modulus = ${E0}
    poissons_ratio = ${nu}
  []
  [stress]
    type = ComputeLinearElasticStress
  []
  [compute_strain]
    type = ComputeSmallStrain
  []
[]

[AuxVariables]
  [vonmises]
    order = CONSTANT
    family = MONOMIAL
  []
[]

[AuxKernels]
  [vonmises]
    type = RankTwoScalarAux
    rank_two_tensor = stress
    variable = vonmises
    scalar_type = VonMisesStress
  []
[]

[Preconditioning]
  [smp]
    type = SMP
    full = true
  []
[]

[Executioner]
  type = Transient
  solve_type = Newton
  petsc_options_iname = '-pc_type -pc_factor_mat_solver_package'
  petsc_options_value = 'lu superlu_dist'
  nl_abs_tol = 1e-8
  dt = 1
  num_steps = 1
[]

[Outputs]
  exodus = true
  # [pgraph]
  #   type = PerfGraphOutput
  #   execute_on = 'initial final'  # Default is "final"
  #   level = 3                     # Default is 1
  #   heaviest_branch = true        # Default is false
  #   heaviest_sections = 7         # Default is 0
  # []
[]

[Debug]
  # show_material_props = true
  # show_execution_order = ALWAYS
[]
