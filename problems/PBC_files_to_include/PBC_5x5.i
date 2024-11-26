[Constraints]
[lr1_disp_x]
type = LinearNodalConstraintFix
variable = disp_x
primary = '3 10 0'
weights = '1 1 -1'
secondary_node_ids = 11
penalty = 1000000.0
formulation = kinematic
[]
[lr1_disp_y]
type = LinearNodalConstraintFix
variable = disp_y
primary = '3 10 0'
weights = '1 1 -1'
secondary_node_ids = 11
penalty = 1000000.0
formulation = kinematic
[]
[lr2_disp_x]
type = LinearNodalConstraintFix
variable = disp_x
primary = '13 10 0'
weights = '1 1 -1'
secondary_node_ids = 17
penalty = 1000000.0
formulation = kinematic
[]
[lr2_disp_y]
type = LinearNodalConstraintFix
variable = disp_y
primary = '13 10 0'
weights = '1 1 -1'
secondary_node_ids = 17
penalty = 1000000.0
formulation = kinematic
[]
[lr3_disp_x]
type = LinearNodalConstraintFix
variable = disp_x
primary = '19 10 0'
weights = '1 1 -1'
secondary_node_ids = 23
penalty = 1000000.0
formulation = kinematic
[]
[lr3_disp_y]
type = LinearNodalConstraintFix
variable = disp_y
primary = '19 10 0'
weights = '1 1 -1'
secondary_node_ids = 23
penalty = 1000000.0
formulation = kinematic
[]
[lr4_disp_x]
type = LinearNodalConstraintFix
variable = disp_x
primary = '25 10 0'
weights = '1 1 -1'
secondary_node_ids = 29
penalty = 1000000.0
formulation = kinematic
[]
[lr4_disp_y]
type = LinearNodalConstraintFix
variable = disp_y
primary = '25 10 0'
weights = '1 1 -1'
secondary_node_ids = 29
penalty = 1000000.0
formulation = kinematic
[]
[lr5_disp_x]
type = LinearNodalConstraintFix
variable = disp_x
primary = '31 10 0'
weights = '1 1 -1'
secondary_node_ids = 35
penalty = 1000000.0
formulation = kinematic
[]
[lr5_disp_y]
type = LinearNodalConstraintFix
variable = disp_y
primary = '31 10 0'
weights = '1 1 -1'
secondary_node_ids = 35
penalty = 1000000.0
formulation = kinematic
[]
[bt1_disp_x]
type = LinearNodalConstraintFix
variable = disp_x
primary = '1 31 0'
weights = '1 1 -1'
secondary_node_ids = 30
penalty = 1000000.0
formulation = kinematic
[]
[bt1_disp_y]
type = LinearNodalConstraintFix
variable = disp_y
primary = '1 31 0'
weights = '1 1 -1'
secondary_node_ids = 30
penalty = 1000000.0
formulation = kinematic
[]
[bt2_disp_x]
type = LinearNodalConstraintFix
variable = disp_x
primary = '4 31 0'
weights = '1 1 -1'
secondary_node_ids = 32
penalty = 1000000.0
formulation = kinematic
[]
[bt2_disp_y]
type = LinearNodalConstraintFix
variable = disp_y
primary = '4 31 0'
weights = '1 1 -1'
secondary_node_ids = 32
penalty = 1000000.0
formulation = kinematic
[]
[bt3_disp_x]
type = LinearNodalConstraintFix
variable = disp_x
primary = '6 31 0'
weights = '1 1 -1'
secondary_node_ids = 33
penalty = 1000000.0
formulation = kinematic
[]
[bt3_disp_y]
type = LinearNodalConstraintFix
variable = disp_y
primary = '6 31 0'
weights = '1 1 -1'
secondary_node_ids = 33
penalty = 1000000.0
formulation = kinematic
[]
[bt4_disp_x]
type = LinearNodalConstraintFix
variable = disp_x
primary = '8 31 0'
weights = '1 1 -1'
secondary_node_ids = 34
penalty = 1000000.0
formulation = kinematic
[]
[bt4_disp_y]
type = LinearNodalConstraintFix
variable = disp_y
primary = '8 31 0'
weights = '1 1 -1'
secondary_node_ids = 34
penalty = 1000000.0
formulation = kinematic
[]
[]
