#pragma once

#include <vector>
#include "libmesh/libmesh_common.h"

using libMesh::Real;

// \returns The norm of a vector
Real NormVec(std::vector<Real> vector);

// \returns The determinant of a matrix
Real getDeterminant(std::vector<std::vector<Real>> mat);

// 1D vector return

// \returns The vector with only absolute values
std::vector<Real> AbsVec(std::vector<Real> vec);

// \returns The mx1 result vector from a multiplication of a mxn matrix and a nx1 vector
std::vector<Real> multMatVec(std::vector<std::vector<Real>> mat, std::vector<Real> vec);

// 2D vector return

// \returns The mxn result matrix from a matrix multiplication of a mxk matrix and a kxn vector
std::vector<std::vector<Real>> multMat(std::vector<std::vector<Real>> mat1,
                                       std::vector<std::vector<Real>> mat2);

// \returns The transpose of a matrix
std::vector<std::vector<Real>> getTranspose(const std::vector<std::vector<Real>> mat);

// \returns The cofactor of a matrix
std::vector<std::vector<Real>> getCofactor(const std::vector<std::vector<Real>> vec);

// \returns The inverse of a matrix
std::vector<std::vector<Real>> getInverse(std::vector<std::vector<Real>> vec);
