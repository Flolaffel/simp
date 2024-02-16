#include "vector.h"
#include "MooseError.h"

/// scalar return
Real
NormVec(std::vector<Real> vector)
{
  Real accum = 0.0;
  for (unsigned int i = 0; i < vector.size(); i++)
  {
    accum += vector[i] * vector[i];
  }
  return std::sqrt(accum);
}

Real
getDeterminant(std::vector<std::vector<Real>> mat)
{
  if (mat.size() != mat[0].size())
  {
    mooseError("Matrix is not quadratic");
  }
  int dimension = mat.size();
  // std::cout << dimension;

  if (dimension == 0)
  {
    return 1;
  }

  if (dimension == 1)
  {
    return mat[0][0];
  }

  // Formula for 2x2-matrix
  if (dimension == 2)
  {
    return mat[0][0] * mat[1][1] - mat[0][1] * mat[1][0];
  }

  Real result = 0;
  for (int x = 0; x < dimension; x++)
  {
    // Submatrix
    std::vector<std::vector<Real>> subMat(dimension - 1, std::vector<Real>(dimension - 1));
    for (int i = 1; i < dimension; i++)
    {
      int z = 0;
      for (int j = 0; j < dimension; j++)
      {
        if (j != x)
        {
          subMat[i - 1][z] = mat[i][j];
          z++;
        }
      }
    }

    // recursive call
    result += std::pow(-1, x) * mat[0][x] * getDeterminant(subMat);
  }

  return result;
}

/// 1D vector return
std::vector<Real>
AbsVec(std::vector<Real> vec)
{
  for (unsigned int i = 0; i < vec.size(); i++)
  {
    if (vec[i] < 0)
      vec[i] *= -1;
  }
  return vec;
}

std::vector<Real>
multMatVec(std::vector<std::vector<Real>> mat, std::vector<Real> vec)
{
  if (mat[0].size() != vec.size())
    mooseError("Matrices have to be of matching dimension for multiplication");

  std::vector<Real> result(mat.size());

  for (unsigned int i = 0; i < mat.size(); i++)
  {
    for (unsigned int k = 0; k < vec.size(); k++)
    {
      result[i] += mat[i][k] * vec[k];
    }
  }
  return result;
}

/// 2D vector return
std::vector<std::vector<Real>>
multMat(std::vector<std::vector<Real>> mat1, std::vector<std::vector<Real>> mat2)
{
  if (mat1[0].size() != mat2.size())
    mooseError("Matrices have to be of matching dimension for multiplication");

  std::vector<std::vector<Real>> result(mat1.size(), std::vector<Real>(mat2[0].size()));

  for (unsigned int i = 0; i < mat1.size(); i++)
  {
    for (unsigned int j = 0; j < mat2[0].size(); j++)
    {
      result[i][j] = 0;
      for (unsigned int k = 0; k < mat2.size(); k++)
      {
        result[i][j] += mat1[i][k] * mat2[k][j];
      }
    }
  }
  return result;
}

std::vector<std::vector<Real>>
getTranspose(const std::vector<std::vector<Real>> mat)
{

  // Transpose-matrix: height = width(matrix), width = height(matrix)
  std::vector<std::vector<Real>> solution(mat[0].size(), std::vector<Real>(mat.size()));

  // Filling solution-matrix
  for (unsigned int i = 0; i < mat.size(); i++)
  {
    for (unsigned int j = 0; j < mat[0].size(); j++)
    {
      solution[j][i] = mat[i][j];
    }
  }
  return solution;
}

std::vector<std::vector<Real>>
getCofactor(const std::vector<std::vector<Real>> vec)
{
  // if (vect.size() != vect[0].size())
  // {
  //   throw std::runtime_error("Matrix is not quadratic");
  // }

  std::vector<std::vector<Real>> solution(vec.size(), std::vector<Real>(vec.size()));
  std::vector<std::vector<Real>> subVec(vec.size() - 1, std::vector<Real>(vec.size() - 1));

  for (unsigned int i = 0; i < vec.size(); i++)
  {
    for (unsigned int j = 0; j < vec[0].size(); j++)
    {
      int p = 0;
      for (unsigned int x = 0; x < vec.size(); x++)
      {
        if (x == i)
        {
          continue;
        }
        int q = 0;

        for (unsigned int y = 0; y < vec.size(); y++)
        {
          if (y == j)
          {
            continue;
          }

          subVec[p][q] = vec[x][y];
          q++;
        }
        p++;
      }
      solution[i][j] = std::pow(-1, i + j) * getDeterminant(subVec);
    }
  }
  return solution;
}

std::vector<std::vector<Real>>
getInverse(std::vector<std::vector<Real>> vec)
{
  // if (getDeterminant(vect) == 0)
  // {
  //   throw std::runtime_error("Determinant is 0");
  // }
  Real d = 1.0 / getDeterminant(vec);
  std::vector<std::vector<Real>> solution(vec.size(), std::vector<Real>(vec.size()));

  solution = getTranspose(getCofactor(vec));

  for (unsigned int i = 0; i < vec.size(); i++)
  {
    for (unsigned int j = 0; j < vec.size(); j++)
    {
      solution[i][j] *= d;
    }
  }

  return solution;
}
