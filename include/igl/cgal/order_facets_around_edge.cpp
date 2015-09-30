#include "order_facets_around_edge.h"
#include <CGAL/Exact_predicates_exact_constructions_kernel.h>

namespace igl
{
  namespace cgal
  {
    namespace order_facets_around_edges_helper
    {
      template<typename T>
        std::vector<size_t> index_sort(const std::vector<T>& data)
        {
          const size_t len = data.size();
          std::vector<size_t> order(len);
          for (size_t i=0; i<len; i++)
          {
            order[i] = i;
          }
          auto comp = [&](size_t i, size_t j)
          {
            return data[i] < data[j];
          };
          std::sort(order.begin(), order.end(), comp);
          return order;
        }
    }
  }
}

// adj_faces contains signed index starting from +- 1.
template<
  typename DerivedV,
  typename DerivedF,
  typename DerivedI >
void igl::cgal::order_facets_around_edge(
  const Eigen::PlainObjectBase<DerivedV>& V,
  const Eigen::PlainObjectBase<DerivedF>& F,
  size_t s, 
  size_t d, 
  const std::vector<int>& adj_faces,
  Eigen::PlainObjectBase<DerivedI>& order)
{
  using namespace igl::cgal::order_facets_around_edges_helper;

  // Although we only need exact predicates in the algorithm,
  // exact constructions are needed to avoid degeneracies due to
  // casting to double.
  typedef CGAL::Exact_predicates_exact_constructions_kernel K;
  typedef K::Point_3 Point_3;
  typedef K::Plane_3 Plane_3;

  auto get_face_index = [&](int adj_f)->size_t
  {
    return abs(adj_f) - 1;
  };

  auto get_opposite_vertex = [&](size_t fid)->size_t
  {
    typedef typename DerivedF::Scalar Index;
    if (F(fid, 0) != (Index)s && F(fid, 0) != (Index)d) return F(fid, 0);
    if (F(fid, 1) != (Index)s && F(fid, 1) != (Index)d) return F(fid, 1);
    if (F(fid, 2) != (Index)s && F(fid, 2) != (Index)d) return F(fid, 2);
    assert(false);
    return -1;
  };

  // Handle base cases
  if (adj_faces.size() == 0) 
  {
    order.resize(0, 1);
    return;
  } else if (adj_faces.size() == 1)
  {
    order.resize(1, 1);
    order(0, 0) = 0;
    return;
  } else if (adj_faces.size() == 2)
  {
    const size_t o1 = get_opposite_vertex(get_face_index(adj_faces[0]));
    const size_t o2 = get_opposite_vertex(get_face_index(adj_faces[1]));
    const Point_3 ps(V(s, 0), V(s, 1), V(s, 2));
    const Point_3 pd(V(d, 0), V(d, 1), V(d, 2));
    const Point_3 p1(V(o1, 0), V(o1, 1), V(o1, 2));
    const Point_3 p2(V(o2, 0), V(o2, 1), V(o2, 2));
    order.resize(2, 1);
    switch (CGAL::orientation(ps, pd, p1, p2))
    {
      case CGAL::POSITIVE:
        order(0, 0) = 1;
        order(1, 0) = 0;
        break;
      case CGAL::NEGATIVE:
        order(0, 0) = 0;
        order(1, 0) = 1;
        break;
      case CGAL::COPLANAR:
        {
          Plane_3 P1(ps, pd, p1);
          Plane_3 P2(ps, pd, p2);
          if (P1.orthogonal_direction() == P2.orthogonal_direction()){
            // Duplicated face, use index to break tie.
            order(0, 0) = adj_faces[0] < adj_faces[1] ? 0:1;
            order(1, 0) = adj_faces[0] < adj_faces[1] ? 1:0;
          } else {
            // Coplanar faces, one on each side of the edge.
            // It is equally valid to order them (0, 1) or (1, 0).
            // I cannot think of any reason to prefer one to the
            // other.  So just use (0, 1) ordering by default.
            order(0, 0) = 0;
            order(1, 0) = 1;
          }
        }
        break;
      default:
        assert(false);
    }
    return;
  }

  const size_t num_adj_faces = adj_faces.size();
  const size_t o = get_opposite_vertex( get_face_index(adj_faces[0]));
  const Point_3 p_s(V(s, 0), V(s, 1), V(s, 2));
  const Point_3 p_d(V(d, 0), V(d, 1), V(d, 2));
  const Point_3 p_o(V(o, 0), V(o, 1), V(o, 2));
  const Plane_3 separator(p_s, p_d, p_o);
  assert(!separator.is_degenerate());

  std::vector<Point_3> opposite_vertices;
  for (size_t i=0; i<num_adj_faces; i++) 
  {
    const size_t o = get_opposite_vertex( get_face_index(adj_faces[i]));
    opposite_vertices.emplace_back(
        V(o, 0), V(o, 1), V(o, 2));
  }

  std::vector<int> positive_side;
  std::vector<int> negative_side;
  std::vector<int> tie_positive_oriented;
  std::vector<int> tie_negative_oriented;

  std::vector<size_t> positive_side_index;
  std::vector<size_t> negative_side_index;
  std::vector<size_t> tie_positive_oriented_index;
  std::vector<size_t> tie_negative_oriented_index;

  for (size_t i=0; i<num_adj_faces; i++)
  {
    const int f = adj_faces[i];
    const Point_3& p_a = opposite_vertices[i];
    auto orientation = separator.oriented_side(p_a);
    switch (orientation) {
      case CGAL::ON_POSITIVE_SIDE:
        positive_side.push_back(f);
        positive_side_index.push_back(i);
        break;
      case CGAL::ON_NEGATIVE_SIDE:
        negative_side.push_back(f);
        negative_side_index.push_back(i);
        break;
      case CGAL::ON_ORIENTED_BOUNDARY:
        {
          const Plane_3 other(p_s, p_d, p_a);
          const auto target_dir = separator.orthogonal_direction();
          const auto query_dir = other.orthogonal_direction();
          if (target_dir == query_dir) {
            tie_positive_oriented.push_back(f);
            tie_positive_oriented_index.push_back(i);
          } else if (target_dir == -query_dir) {
            tie_negative_oriented.push_back(f);
            tie_negative_oriented_index.push_back(i);
          } else {
            assert(false);
          }
        }
        break;
      default:
        // Should not be here.
        assert(false);
    }
  }

  Eigen::PlainObjectBase<DerivedI> positive_order, negative_order;
  order_facets_around_edge(V, F, s, d, positive_side, positive_order);
  order_facets_around_edge(V, F, s, d, negative_side, negative_order);
  std::vector<size_t> tie_positive_order = index_sort(tie_positive_oriented);
  std::vector<size_t> tie_negative_order = index_sort(tie_negative_oriented);

  // Copy results into order vector.
  const size_t tie_positive_size = tie_positive_oriented.size();
  const size_t tie_negative_size = tie_negative_oriented.size();
  const size_t positive_size = positive_order.size();
  const size_t negative_size = negative_order.size();

  order.resize(
    tie_positive_size + positive_size + tie_negative_size + negative_size,1);

  size_t count=0;
  for (size_t i=0; i<tie_positive_size; i++)
  {
    order(count+i, 0) = tie_positive_oriented_index[tie_positive_order[i]];
  }
  count += tie_positive_size;

  for (size_t i=0; i<negative_size; i++)
  {
    order(count+i, 0) = negative_side_index[negative_order(i, 0)];
  }
  count += negative_size;

  for (size_t i=0; i<tie_negative_size; i++)
  {
    order(count+i, 0) = tie_negative_oriented_index[tie_negative_order[i]];
  }
  count += tie_negative_size;

  for (size_t i=0; i<positive_size; i++)
  {
    order(count+i, 0) = positive_side_index[positive_order(i, 0)];
  }
  count += positive_size;
  assert(count == num_adj_faces);

  // Find the correct start point.
  size_t start_idx = 0;
  for (size_t i=0; i<num_adj_faces; i++)
  {
    const Point_3& p_a = opposite_vertices[order(i, 0)];
    const Point_3& p_b =
      opposite_vertices[order((i+1)%num_adj_faces, 0)];
    auto orientation = CGAL::orientation(p_s, p_d, p_a, p_b);
    if (orientation == CGAL::POSITIVE)
    {
      // Angle between triangle (p_s, p_d, p_a) and (p_s, p_d, p_b) is
      // more than 180 degrees.
      start_idx = (i+1)%num_adj_faces;
      break;
    } else if (orientation == CGAL::COPLANAR &&
        Plane_3(p_s, p_d, p_a).orthogonal_direction() !=
        Plane_3(p_s, p_d, p_b).orthogonal_direction())
    {
      // All 4 points are coplanar, but p_a and p_b are on each side of
      // the edge (p_s, p_d).  This means the angle between triangle
      // (p_s, p_d, p_a) and (p_s, p_d, p_b) is exactly 180 degrees.
      start_idx = (i+1)%num_adj_faces;
      break;
    }
  }
  DerivedI circular_order = order;
  for (size_t i=0; i<num_adj_faces; i++)
  {
    order(i, 0) = circular_order((start_idx + i)%num_adj_faces, 0);
  }
}

template<
  typename DerivedV,
  typename DerivedF,
  typename DerivedI>
IGL_INLINE
void igl::cgal::order_facets_around_edge(
  const Eigen::PlainObjectBase<DerivedV>& V,
  const Eigen::PlainObjectBase<DerivedF>& F,
  size_t s, 
  size_t d, 
  const std::vector<int>& adj_faces,
  const Eigen::PlainObjectBase<DerivedV>& pivot_point,
  Eigen::PlainObjectBase<DerivedI>& order) 
{

  assert(V.cols() == 3);
  assert(F.cols() == 3);
  auto signed_index_to_index = [&](int signed_idx)
  {
    return abs(signed_idx) -1;
  };
  auto get_opposite_vertex_index = [&](size_t fid)
  {
    typedef typename DerivedF::Scalar Index;
    if (F(fid, 0) != (Index)s && F(fid, 0) != (Index)d) return F(fid, 0);
    if (F(fid, 1) != (Index)s && F(fid, 1) != (Index)d) return F(fid, 1);
    if (F(fid, 2) != (Index)s && F(fid, 2) != (Index)d) return F(fid, 2);
    assert(false);
    // avoid warning
    return -1;
  };

  const typename DerivedF::Scalar N = adj_faces.size();
  const size_t num_faces = N + 1; // N adj faces + 1 pivot face

  DerivedV vertices(num_faces + 2, 3);
  for (size_t i=0; i<(size_t)N; i++)
  {
    const size_t fid = signed_index_to_index(adj_faces[i]);
    vertices.row(i) = V.row(get_opposite_vertex_index(fid));
  }
  vertices.row(N  ) = pivot_point;
  vertices.row(N+1) = V.row(s);
  vertices.row(N+2) = V.row(d);

  DerivedF faces(num_faces, 3);
  for (size_t i=0; i<(size_t)N; i++) 
  {
    if (adj_faces[i] < 0)
    {
      faces(i,0) = N+1; // s
      faces(i,1) = N+2; // d
      faces(i,2) = i  ;
    } else {
      faces(i,0) = N+2; // d
      faces(i,1) = N+1; // s
      faces(i,2) = i  ;
    }
  }
  // Last face is the pivot face.
  faces(N, 0) = N+1;
  faces(N, 1) = N+2;
  faces(N, 2) = N;

  std::vector<int> adj_faces_with_pivot(num_faces);
  for (size_t i=0; i<num_faces; i++)
  {
    if (faces(i,0) == N+1 && faces(i,1) == N+2)
    {
      adj_faces_with_pivot[i] = int(i+1) * -1;
    } else
    {
      adj_faces_with_pivot[i] = int(i+1);
    }
  }

  DerivedI order_with_pivot;
  igl::cgal::order_facets_around_edge(
      vertices, faces, N+1, N+2,
      adj_faces_with_pivot, order_with_pivot);

  assert(order_with_pivot.size() == num_faces);
  order.resize(N);
  size_t pivot_index = num_faces + 1;
  for (size_t i=0; i<num_faces; i++)
  {
    if (order_with_pivot[i] == N)
    {
      pivot_index = i;
      break;
    }
  }
  assert(pivot_index < num_faces);

  for (size_t i=0; i<(size_t)N; i++)
  {
    order[i] = order_with_pivot[(pivot_index+i+1)%num_faces];
  }
}


#ifdef IGL_STATIC_LIBRARY
// Explicit template specialization
template void igl::cgal::order_facets_around_edge<Eigen::Matrix<CGAL::Lazy_exact_nt<CGAL::Gmpq>, -1, 3, 0, -1, 3>, Eigen::Matrix<int, -1, 3, 0, -1, 3>, Eigen::Matrix<int, -1, 1, 0, -1, 1> >(Eigen::PlainObjectBase<Eigen::Matrix<CGAL::Lazy_exact_nt<CGAL::Gmpq>, -1, 3, 0, -1, 3> > const&, Eigen::PlainObjectBase<Eigen::Matrix<int, -1, 3, 0, -1, 3> > const&, unsigned long, unsigned long, std::__1::vector<int, std::__1::allocator<int> > const&, Eigen::PlainObjectBase<Eigen::Matrix<int, -1, 1, 0, -1, 1> >&);
template void igl::cgal::order_facets_around_edge<Eigen::Matrix<double, -1, 3, 0, -1, 3>, Eigen::Matrix<int, -1, 3, 0, -1, 3>, Eigen::Matrix<int, -1, 1, 0, -1, 1> >(Eigen::PlainObjectBase<Eigen::Matrix<double, -1, 3, 0, -1, 3> > const&, Eigen::PlainObjectBase<Eigen::Matrix<int, -1, 3, 0, -1, 3> > const&, unsigned long, unsigned long, std::__1::vector<int, std::__1::allocator<int> > const&, Eigen::PlainObjectBase<Eigen::Matrix<int, -1, 1, 0, -1, 1> >&);
template void igl::cgal::order_facets_around_edge<Eigen::Matrix<double, -1, -1, 0, -1, -1>, Eigen::Matrix<int, -1, -1, 0, -1, -1>, Eigen::Matrix<int, -1, 1, 0, -1, 1> >(Eigen::PlainObjectBase<Eigen::Matrix<double, -1, -1, 0, -1, -1> > const&, Eigen::PlainObjectBase<Eigen::Matrix<int, -1, -1, 0, -1, -1> > const&, unsigned long, unsigned long, std::__1::vector<int, std::__1::allocator<int> > const&, Eigen::PlainObjectBase<Eigen::Matrix<int, -1, 1, 0, -1, 1> >&);
template void igl::cgal::order_facets_around_edge<Eigen::Matrix<CGAL::Lazy_exact_nt<CGAL::Gmpq>, -1, 3, 0, -1, 3>, Eigen::Matrix<int, -1, 3, 0, -1, 3>, Eigen::Matrix<int, -1, 1, 0, -1, 1> >(Eigen::PlainObjectBase<Eigen::Matrix<CGAL::Lazy_exact_nt<CGAL::Gmpq>, -1, 3, 0, -1, 3> > const&, Eigen::PlainObjectBase<Eigen::Matrix<int, -1, 3, 0, -1, 3> > const&, unsigned long, unsigned long, std::__1::vector<int, std::__1::allocator<int> > const&, Eigen::PlainObjectBase<Eigen::Matrix<CGAL::Lazy_exact_nt<CGAL::Gmpq>, -1, 3, 0, -1, 3> > const&, Eigen::PlainObjectBase<Eigen::Matrix<int, -1, 1, 0, -1, 1> >&);
template void igl::cgal::order_facets_around_edge<Eigen::Matrix<double, -1, 3, 0, -1, 3>, Eigen::Matrix<int, -1, 3, 0, -1, 3>, Eigen::Matrix<int, -1, 1, 0, -1, 1> >(Eigen::PlainObjectBase<Eigen::Matrix<double, -1, 3, 0, -1, 3> > const&, Eigen::PlainObjectBase<Eigen::Matrix<int, -1, 3, 0, -1, 3> > const&, unsigned long, unsigned long, std::__1::vector<int, std::__1::allocator<int> > const&, Eigen::PlainObjectBase<Eigen::Matrix<double, -1, 3, 0, -1, 3> > const&, Eigen::PlainObjectBase<Eigen::Matrix<int, -1, 1, 0, -1, 1> >&);
template void igl::cgal::order_facets_around_edge<Eigen::Matrix<double, -1, -1, 0, -1, -1>, Eigen::Matrix<int, -1, -1, 0, -1, -1>, Eigen::Matrix<int, -1, 1, 0, -1, 1> >(Eigen::PlainObjectBase<Eigen::Matrix<double, -1, -1, 0, -1, -1> > const&, Eigen::PlainObjectBase<Eigen::Matrix<int, -1, -1, 0, -1, -1> > const&, unsigned long, unsigned long, std::__1::vector<int, std::__1::allocator<int> > const&, Eigen::PlainObjectBase<Eigen::Matrix<double, -1, -1, 0, -1, -1> > const&, Eigen::PlainObjectBase<Eigen::Matrix<int, -1, 1, 0, -1, 1> >&);
#endif