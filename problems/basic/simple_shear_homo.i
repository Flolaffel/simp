# einfache homogene Scherung

[Mesh]
  type = GeneratedMesh
  dim = 2
  nx = 1
  ny = 1
[]

[GlobalParams]
  displacements = 'disp_x disp_y'
[]

[Modules/TensorMechanics/Master]
  [all]
    strain = SMALL
    add_variables = true
    incremental = false
    generate_output = "stress_xx stress_yy stress_xy stress_zz strain_xx strain_yy strain_xy "
                      "strain_zz"
  []
[]

[Functions]
  [disp_quer]
    type = ParsedFunction
    value = 'y'
  []
[]

[BCs]
  [bottom_y]
    type = DirichletBC
    variable = disp_y
    boundary = bottom
    value = 0
  []
  [bottom_x]
    type = DirichletBC
    variable = disp_x
    boundary = bottom
    value = 0
  []
  [top_y]
    type = DirichletBC
    variable = disp_y
    boundary = top
    value = 0
  []
  [left_y]
    type = DirichletBC
    variable = disp_y
    boundary = left
    value = 0
  []
  [right_y]
    type = DirichletBC
    variable = disp_y
    boundary = right
    value = 0
  []
  [tdisp]
    type = DirichletBC
    variable = disp_x
    boundary = top
    value = 1
  []
  [tdisp_left_x]
    type = FunctionDirichletBC
    variable = disp_x
    boundary = left
    function = disp_quer
  []
  [tdisp_right_z]
    type = FunctionDirichletBC
    variable = disp_x
    boundary = right
    function = disp_quer
  []
[]

[Materials]
  [elasticity_tensor]
    type = ComputeIsotropicElasticityTensor
    youngs_modulus = 0.1250000009
    poissons_ratio = 0.3
  []
  [stress]
    type = ComputeLinearElasticStress
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
