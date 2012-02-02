#include "per_corner_normals.h"

#include "vf.h"
#include "per_face_normals.h"
#include "PI.h"

template <typename DerivedV, typename DerivedF>
IGL_INLINE void igl::per_corner_normals(
                                   const Eigen::PlainObjectBase<DerivedV>& V,
                                   const Eigen::PlainObjectBase<DerivedF>& F,
                                   const double corner_threshold,
                                   Eigen::PlainObjectBase<DerivedV> & CN)
{
  using namespace igl;
  using namespace Eigen;
  using namespace std;
  Eigen::PlainObjectBase<DerivedV> FN;
  per_face_normals(V,F,FN);
  vector<vector<int> > VF,VFi;
  vf(V,F,VF,VFi);
  return per_corner_normals(V,F,FN,VF,corner_threshold,CN);
}

template <typename DerivedV, typename DerivedF, typename IndexType>
IGL_INLINE void igl::per_corner_normals(
                                   const Eigen::PlainObjectBase<DerivedV>& V,
                                   const Eigen::PlainObjectBase<DerivedF>& F,
                                   const Eigen::PlainObjectBase<DerivedV>& FN,
                                   const std::vector<std::vector<IndexType> >& VF,
                                   const double corner_threshold,
                                   Eigen::PlainObjectBase<DerivedV> & CN)
{
  using namespace igl;
  using namespace Eigen;
  using namespace std;

  // number of faces
  const int m = F.rows();
  // valence of faces
  const int n = F.cols();

  // initialize output to zero
  CN.resize(m*n,3);

  // loop over faces
  for(size_t i = 0;i<m;i++)
  {
    // Normal of this face
    Eigen::Matrix<typename DerivedV::Scalar,3,1> fn = FN.row(i);
    // loop over corners
    for(size_t j = 0;j<n;j++)
    {
      const std::vector<IndexType> &incident_faces = VF[F(i,j)];
      // loop over faces sharing vertex of this corner
      for(int k = 0;k<(int)incident_faces.size();k++)
      {
        Eigen::Matrix<typename DerivedV::Scalar,3,1> ifn = FN.row(incident_faces[k]);
        // dot product between face's normal and other face's normal
        double dp = fn.dot(ifn);
        // if difference in normal is slight then add to average
        if(dp > cos(corner_threshold*PI/180))
        {
          // add to running sum
          CN.row(i*n+j) += ifn;
        // else ignore
        }else
        {
        }
      }
      // normalize to take average
      CN.row(i*n+j).normalize();
    }
  }
}