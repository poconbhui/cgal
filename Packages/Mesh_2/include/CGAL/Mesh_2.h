#ifndef CGAL_MESH_2_H
#define CGAL_MESH_2_H
#include <CGAL/basic.h>
#include <list>
#include <map>
#include <queue>
#include <iostream>
#include <fstream>
#include <CGAL/Constrained_Delaunay_triangulation_2.h>
#include <CGAL/utility.h>
#include <CGAL/iterator.h>
#include <CGAL/Filtred_container.h>
#include <CGAL/Filtred_circulator.h>
#include <CGAL/Double_map.h>

#include <CGAL/IO/File_header_extended_OFF.h> 
// to skip comments and EOF in the function read_poly

#ifdef CGAL_MESH_2_USE_TIMERS
#include <CGAL/Timer.h>
#endif

CGAL_BEGIN_NAMESPACE

// Tr is a Delaunay constrained triangulation (with intersections or not)
template <class Tr>
class Mesh_2: public Tr
{
public:
  typedef Tr Triangulation;
  typedef Mesh_2<Triangulation> Self;
  
  typedef typename Tr::Geom_traits Geom_traits;
  typedef typename Tr::Triangulation_data_structure Tds;
  typedef typename Geom_traits::FT FT;
  typedef FT      Squared_length;

  typedef typename Tr::Vertex                 Vertex;
  typedef typename Tr::Edge                   Edge;
  typedef typename Tr::Finite_edges_iterator  Finite_edges_iterator;
  typedef typename Tr::Edge_circulator        Edge_circulator;
  typedef typename Tr::Face_handle            Face_handle;
  typedef typename Tr::Finite_faces_iterator  Finite_faces_iterator;
  typedef typename Tr::All_faces_iterator  All_faces_iterator;
  typedef typename Tr::Face_circulator        Face_circulator;
  typedef typename Tr::Vertex_handle          Vertex_handle;
  typedef typename Tr::Vertex_circulator      Vertex_circulator;
  typedef typename Tr::Finite_vertices_iterator Finite_vertices_iterator;

  typedef typename Tr::Locate_type            Locate_type;
  
  typedef typename Tr::Point                  Point;

  typedef typename Tr::List_constraints       List_constraints;

public:
  // CONSTRUCTORS

  // default constructor, to be used with IO functions
  Mesh_2(const Geom_traits& gt = Geom_traits());

  // constructor from a Triangulation& t
  explicit 
  Mesh_2(Triangulation& t, const Geom_traits& gt = Geom_traits(),
	 bool dont_refine = false);

  // the same with declaration of seeds.
  // See the explanation of functions refine(...) for the
  // signification of arguments Seed_it begin,end and bool mark.
  // The parameter dont_refine should not be documented because it
  // allow to construct a "mesh" that is not meshed.
  template <class Seed_it> 
  explicit 
  Mesh_2(Triangulation& t, Seed_it begin, Seed_it end, 
	 const Geom_traits& gt = Geom_traits(), bool mark = false,
	 bool dont_refine = false)
    {
      swap(t);
      if(!dont_refine)
	refine(begin, end, mark);
    };

  // ASSIGNEMENT
  // TODO!

  // ACCESS FUNCTIONS
  unsigned int number_of_constrained_edges() const;

  bool is_bad(const Face_handle fh) const;

  double squared_minimum_sine(const Face_handle fh) const;
  double squared_minimum_sine(const Vertex_handle& va,
			      const Vertex_handle& vb,
			      const Vertex_handle& vc) const;
  // IO

  // write and read the constrained edges in the format:
  //   number_of_edges
  //   segment1
  //   segment2
  //   ...
  void write(std::ostream &f) const;
  void read(std::istream &f, bool dont_refine = false);

  // write and read a mesh in the Triangle .poly format 
  // (see http://www-2.cs.cmu.edu/~quake/triangle.poly.html)
  void write_poly(std::ostream &f) const;
  void read_poly(std::istream &f, bool dont_refine = false);

  // HELPING FUNCTION
  void clear();

  // MESHING FUNCTIONS

  // Perform meshing. All faces but those connected to the infinite
  // faces are marked.
  void refine();

  // Perform meshing. Seed_it is an iterator of points, representing
  // seeds. Connected components of seeds are marked with the value of 
  // "mark". Other components are marked with !mark. The connected
  // component of infinite faces is always marked with false.
  template <class Seed_it>
  void refine(Seed_it begin, Seed_it end, bool mark = false)
    {
      init(begin, end, mark);

#ifdef CGAL_MESH_2_USE_TIMERS
      timer.reset();
      timer.start();
#endif
      while(! (edges_to_be_conformed.empty() && bad_faces.empty()) )
	{
	  conform();
	  if ( !bad_faces.empty() )
	    process_one_face();
	}
#ifdef CGAL_MESH_2_USE_TIMERS
      timer.stop();
      std::cout << "Refine mesh time: " << timer.time() << std::endl;
#endif
    }

  // REMESHING FUNCTIONS

  // Set the geom_traits and recalculate the list of bad faces
  void set_geom_traits(const Geom_traits& gt);

  // Set the geom_traits and add the sequence [begin, end[ to the list
  // of bad faces.
  // Fh_it is a iterator of Face_Handle.
  // Use this overriden function if the list of bad faces can be
  // computed easily without testing all faces.
  template <class Fh_it>
  void set_geom_traits(const Geom_traits& gt,
		       Fh_it begin, Fh_it end)
  {
    _gt = gt;
    for(Fh_it pfit=begin; pfit!=end; ++pfit)
      push_in_bad_faces(*pfit);
  }

  // STEP BY STEP MESHING

  // two deprecated constructors

  Mesh_2(List_constraints& lc, const Geom_traits& gt = Geom_traits());

  template <class InputIterator>
  Mesh_2(InputIterator first, InputIterator last, 
	 const Geom_traits& gt = Geom_traits())
    : Tr(gt), is_really_a_constrained_edge(*this), 
      edges_to_be_conformed(is_really_a_constrained_edge)
    {
      while(first != last){
	insert_constraint((*first).first, (*first).second);
	++first;
      }
      CGAL_triangulation_postcondition(is_valid());
    }

  // init(...): Initialize the data structures 
  // (The call of one of the following init function is REQUIRED
  // before any step by step operation).
  // See the explanations of functions refine(...) for the
  // signification of arguments
  void init();

  template <class Seed_it> 
  void init(Seed_it begin, Seed_it end, bool mark = false)
    {
      cluster_map.clear();
      edges_to_be_conformed.clear();
      bad_faces.clear();
      
      mark_facets(begin, end, mark);
      
      create_clusters();
      fill_edge_queue();
      fill_facette_map();
    }

  // mark_facets(...): procedure called to mark facets, same arguments 
  // as init(...) above
  template <class Seed_it>
  void mark_facets(Seed_it begin, Seed_it end, bool mark = false)
    {
      if (dimension()<2) return;
      if(begin!=end)
	{
	  for(All_faces_iterator it=all_faces_begin();
	      it!=all_faces_end();
	      ++it)
	    it->set_marked(!mark);
	  
	  for(Seed_it sit=begin; sit!=end; ++sit)
	    {
	      Face_handle fh=locate(*sit);
	      if(fh!=NULL)
		propagate_marks(fh, mark);
	    }
	}
      else
	mark_convex_hull();
      propagate_marks(infinite_face(), false);
    };

  // Conform edges and doesn't refine faces
  // Need init(...) see bellow
  void conform();

  // Execute on step of the algorithm.
  // Need init(...) see bellow
  bool refine_step();

private:
  // PRIVATE TYPES
  typedef std::pair<Vertex_handle,Vertex_handle>
                                              Constrained_edge;
  typedef std::list<Edge> List_of_edges;
  typedef std::list<Face_handle> List_of_face_handles;
  typedef CGAL::Triple<Vertex_handle,
                       Vertex_handle,
                       Vertex_handle> Threevertices;
  // traits type
  typedef typename Geom_traits::Vector_2 Vector_2;
  typedef typename Geom_traits::Construct_translated_point_2
      Construct_translated_point_2;
  typedef typename Geom_traits::Compute_squared_distance_2
      Compute_squared_distance_2;
  typedef typename Geom_traits::Angle_2 Angle_2;
  typedef typename Geom_traits::Construct_vector_2
      Construct_vector_2;
  typedef typename Geom_traits::Construct_scaled_vector_2
      Construct_scaled_vector_2;
  typedef typename Geom_traits::Construct_midpoint_2
      Construct_midpoint_2;
  typedef typename Geom_traits::Orientation_2 Orientation_2;
  typedef typename Geom_traits::Compute_squared_minimum_sine_2 
      Compute_squared_minimum_sine_2;
  typedef typename Geom_traits::Is_bad Is_bad;

  // Cluster register several informations about clusters.
  // A cluster is a is a set of vertices v_i incident to one vertice
  // v_0, so that angles between segments [v_0, v_i] is less than 60�.
  struct Cluster {
    bool reduced ; // Is the cluster reduced

    // smallest_angle gives the two vertices defining the
    // smallest angle in the cluster
    std::pair<Vertex_handle, Vertex_handle> smallest_angle;

    FT rmin; // WARNING: rmin has no meaning if reduced=false!!!
    Squared_length minimum_squared_length;

    // The following map tells what vertices are in the cluster and if 
    // the corresponding segment has been splitted once.
    typedef std::map<Vertex_handle, bool> Vertices_map;
    Vertices_map vertices;

    bool is_reduced() const {
      return reduced;
    }

    bool is_reduced(const Vertex_handle v) {
      return vertices[v];
    }
  };

  // Is_edge_constrained: function object class.
  // Take a Mesh_2 object m and a Vertex_handle v in constructor.
  // Is_edge_constrained(Vertex_handle v2) tells if [v,v2] is a
  // constrained edge in m.
  class Is_edge_constrained {
    Self* _m;
    Vertex_handle _v;
  public:
    Is_edge_constrained(Self* m, Vertex_handle v)
      : _m(m), _v(v) {}

    Is_edge_constrained(const Is_edge_constrained& other)
      : _m(other._m), _v(other._v) {}

    Is_edge_constrained& 
    operator=(const Is_edge_constrained& other)
      {
	_m = other._m;
	_v = other._v;
	return *this;
      }

    bool operator()(Vertex& v2) const 
      {
	if(_m->is_infinite(v2.handle()))
	  return false;
	Face_handle fh;
	int i;
	_m->is_edge(_v,v2.handle(),fh,i);
	return fh->is_constrained(i);
      }
  };

  class Is_really_a_constrained_edge {
    const Self& _m;
  public:
    explicit Is_really_a_constrained_edge(const Self& m) : _m(m) {};
    bool operator()(const Constrained_edge& ce) const
      {
	Face_handle fh;
	int i;
	return _m.is_edge(ce.first, ce.second, fh,i) &&
	  fh->is_constrained(i);
      }
  };

  // typedefs for private members types
  typedef Filtred_circulator<Vertex_circulator, Is_edge_constrained>
    Constrained_vertex_circulator;

  typedef CGAL::Double_map<Face_handle, double> Bad_faces;

  typedef std::list<Constrained_edge> List_of_constraints;
  typedef CGAL::Filtred_container<List_of_constraints, 
                                  Is_really_a_constrained_edge>
    Constrained_edges_queue;

  typedef std::multimap<Vertex_handle, Cluster> Cluster_map;

private:
  // PRIVATE MEMBER DATAS

  // bad_faces: list of bad finite faces
  // warning: some faces could be recycled during insertion in the
  //  triangulation, that's why we need to be able to remoce faces
  //  from the map.
  Bad_faces bad_faces;

  // edges_to_be_conformed: list of encroached constrained edges
  //  warning: some edges could be destroyed, use the same wrapper
  // is_really_a_constrained_edge: tester to filter edges_to_be_conformed
  const Is_really_a_constrained_edge is_really_a_constrained_edge;
  Constrained_edges_queue edges_to_be_conformed;

  // Cluster_map: multimap Vertex_handle -> Cluster
  // each vertex can have several clusters
  Cluster_map cluster_map;

#ifdef CGAL_MESH_2_USE_TIMERS
  // Timer to bench
  Timer timer;
#endif

public:
  // PUBLIC DATA MEMBERS
  typedef std::list<Point> Seeds;
  Seeds seeds; // TODO, WARNING: have to think about this variable and 
  // about the default marker.

private: 
  // PRIVATE MEMBER FUNCTIONS

  // -- auxiliary functions to set markers --

  // mark all faces of the convex_hull but those connected to the
  // infinite faces
  void mark_convex_hull();

  // propagate the mark m recursivly
  void propagate_marks(Face_handle f, bool m);

  // -- auxiliary functions to handle clusters --

  // for all vertices, call create_clusters_of_vertex
  void create_clusters();

  // compute clusters of the vertex v, using the auxiliary function
  // construct_cluster
  void create_clusters_of_vertex(Vertex_handle v);

  // add the sequence [begin, end] to the cluster c and add it to the
  // clusters of the vertex v
  void construct_cluster(Vertex_handle v,
			 Constrained_vertex_circulator begin,
			 const Constrained_vertex_circulator& end,
			 Cluster c = Cluster());

  // update the cluster of [va,vb], putting vm instead of vb. If
  // reduction=false, the edge [va,vm] is not set reduced.
  void update_cluster(Cluster& c, Vertex_handle va, Vertex_handle vb,
		      Vertex_handle vm, bool reduction = true);

  // get_cluster returns the cluster of [va,vb] in c and return true
  // if it is in a cluster. If erase=true, the cluster is remove from
  // the cluster map
  bool get_cluster(Vertex_handle va, Vertex_handle vb, Cluster &c,
		   bool erase = false);

  // -- functions that maintain the queue of encroached edges --

  // scan all constrained edges and put them in the queue if they are
  // encroached
  void fill_edge_queue();

  // update the queue with edges incident to vm
  void update_edges_to_be_conformed(Vertex_handle va,
			   Vertex_handle vb,
			   Vertex_handle vm);

  // -- functions that maintain the map of bad faces

  // auxiliary functions called to put a new face in the map, two
  // forms
  void push_in_bad_faces(Face_handle fh);
  void push_in_bad_faces(Vertex_handle va,
			 Vertex_handle vb,
			 Vertex_handle vc);
 
  // scan all faces and put them if needed in the map
  void fill_facette_map();

  // update the map with faces incident to the vertex v
  void compute_new_bad_faces(Vertex_handle v);

  // -- inlined functions that compose the refinement process --

  // take one edge in the queue and call refine_edge
  void process_one_edge();

  // take one face in the queue and call refine_face
  void process_one_face();

  // handle the encroached edge, call cut_cluster_edge+update_cluster
  // or insert_middle
  void refine_edge(Vertex_handle va, Vertex_handle vb);

  // handle one face; call split_face or put in the edges_to_be_conformed the
  // list of edges that would be encroached by the circum_center of f
  // This function uses Shewchuk's terminator criteria.
  void refine_face(Face_handle f);

  // -- auxiliary functions that return a boolean --

  // tell if [va,vb] is encroached, by looking for the two neighbors
  // This function takes of markers.
  bool is_encroached(const Vertex_handle va, 
		     const Vertex_handle vb) const;

  // tell if [va,vb] is encroached by p
  bool is_encroached(const Vertex_handle va, 
		     const Vertex_handle vb,
		     const Point& p) const;

  // tell if the angle <pleft, pmiddle, pright> is less than 60�
  // Uses squared_cosine_of_angle_times_4 and used by
  // create_clusters_of_vertex
  bool is_small_angle(const Point& pleft,
		      const Point& pmiddle, 
		      const Point& pright) const;

  // -- auxiliary functions that are called to split an edge or --
  // -- a face                                                  --

  // cut [va,vb] knowing that it is in the cluster c
  void cut_cluster_edge(Vertex_handle va, Vertex_handle vb,
			Cluster& c);

  // insert the midpoint of the edge (f,i)
  Vertex_handle insert_middle(Face_handle f, int i);

  // -- functions that really insert points --

  // split the face f by inserting its circum center circum_center
  void split_face(const Face_handle& f, const Point& circum_center);

  // insert the point p in the edge (fh,edge_index)
  Vertex_handle insert_in_the_edge(Face_handle fh, int edge_index,
				   const Point& p);
  
  // -- helping computing functions -- 

  // return the squared cosine of the angle <pleft, pmiddle, pright>
  // times 4
  FT squared_cosine_of_angle_times_4(const Point& pleft,
				     const Point& pmiddle,
				     const Point& pright) const;

  // return the squared length of the triangle corresponding to the
  // face f
  Squared_length shortest_edge_squared_length(Face_handle f);

  // -- debugging functions --
  bool is_bad_faces_valid();
}; // end of Mesh_2

// CONSTRUCTORS

template <class Tr>
Mesh_2<Tr>::
Mesh_2(const Geom_traits& gt)
  : Tr(gt), is_really_a_constrained_edge(*this),
    edges_to_be_conformed(is_really_a_constrained_edge)
{};

template <class Tr>
Mesh_2<Tr>::
Mesh_2(Triangulation& t, const Geom_traits& gt, bool dont_refine)
  : Tr(gt), is_really_a_constrained_edge(*this),
    edges_to_be_conformed(is_really_a_constrained_edge)
{
  swap(t);
  if(!dont_refine)
    refine();
};

// ACCESS FUNCTIONS

template <class Tr>
unsigned int Mesh_2<Tr>::
number_of_constrained_edges() const
{
  int nedges = 0;
  for(Finite_edges_iterator eit = finite_edges_begin();
      eit != finite_edges_end();
      ++eit)
    if((*eit).first->is_constrained((*eit).second))
      ++nedges;
  return nedges;
}

// ?????????????
// ->traits
// the measure of faces quality
// # We can add here other contraints, such as a bound on the size
template <class Tr>
inline
bool Mesh_2<Tr>::
is_bad(const Face_handle f) const
{
  const Point
    & a = f->vertex(0)->point(),
    & b = f->vertex(1)->point(),
    & c = f->vertex(2)->point();

  return geom_traits().is_bad_object()(a,b,c);
}

template <class Tr>
inline
double Mesh_2<Tr>::
squared_minimum_sine(const Vertex_handle& va, const Vertex_handle& vb,
		     const Vertex_handle& vc) const
{
  Compute_squared_minimum_sine_2 squared_sine = 
    geom_traits().compute_squared_minimum_sine_2_object();
  return squared_sine(va->point(), vb->point(), vc->point());
}

template <class Tr>
inline
double Mesh_2<Tr>::
squared_minimum_sine(const Face_handle fh) const
{
  const Vertex_handle
    & va = fh->vertex(0),
    & vb = fh->vertex(1),
    & vc = fh->vertex(2);
  return squared_minimum_sine(va, vb, vc);
}

// IO
//the function that writes a file
template <class Tr>
void Mesh_2<Tr>::
write(std::ostream &f) const
{
  f << number_of_constrained_edges() << std::endl;
  for(Finite_edges_iterator eit = finite_edges_begin();
      eit!=finite_edges_end();
      ++eit)
    if((*eit).first->is_constrained((*eit).second)) 
      {
	f << (*eit).first->vertex(cw((*eit).second))->point() << " "
	  << (*eit).first->vertex(ccw((*eit).second))->point() <<std::endl;
      }
}

//the function that reads a file
template <class Tr>
void Mesh_2<Tr>::
read(std::istream &f, bool dont_refine)
{
  int nedges = 0;
  clear();
  f>>nedges;
  for(int n = 0; n<nedges; n++) {
    Point p1, p2;
    f >> p1 >> p2;
    insert_constraint(p1, p2);
  }
  if(!dont_refine)
    refine();
}

//the function that write a Shewchuk Triangle .poly file
template <class Tr>
void Mesh_2<Tr>::
write_poly(std::ostream &f) const
{
  std::map<Vertex_handle, unsigned int> index;

  // write vertices
  f << "# Shewchuk Triangle .poly file, produced by the CGAL::Mesh_2 package"
    << std::endl
    << "# Neither attributes nor boundary markers are used." << std::endl
    << number_of_vertices() << " " << 2 << " " 
    << 0 << " " << 0 << std::endl;

  f << std::endl;

  unsigned int vertices_counter = 0;
  for(Finite_vertices_iterator vit = finite_vertices_begin();
      vit != finite_vertices_end();
      ++vit)
    {
      f << ++vertices_counter << " " << vit->point() << std::endl;
      index[vit] = vertices_counter;
    }

  f << std::endl;

  // write constrained edges

  f << number_of_constrained_edges() << " " << 0 << std::endl;
  unsigned int edges_counter = 0;
  for(Finite_edges_iterator eit = finite_edges_begin();
      eit != finite_edges_end();
      ++eit)
    if((*eit).first->is_constrained((*eit).second)) 
      f << ++edges_counter << " "
	<< index[(*eit).first->vertex(cw((*eit).second))] << " "
	<< index[(*eit).first->vertex(ccw((*eit).second))] 
	<< std::endl;

  f << std::endl;

  // write seeds, assuming that the seeds unmarks faces
  unsigned int seeds_counter = 0;
  f << seeds.size() << std::endl;
  for(typename Seeds::const_iterator sit = seeds.begin();
      sit!=seeds.end(); ++sit)
    f << ++seeds_counter << " " << *sit << std::endl;
}

//the function that reads a Shewchuk Triangle .poly file
template <class Tr>
void Mesh_2<Tr>::
read_poly(std::istream &f, bool dont_refine)
{
  clear();

  unsigned int number_of_points;
  skip_comment_OFF(f);
  f >> number_of_points;
  skip_until_EOL(f);
  skip_comment_OFF(f);
  
  // read vertices
  std::vector<Vertex_handle> vertices(number_of_points);
  for(unsigned int i = 0; i < number_of_points; ++i)
    {
      unsigned int j;
      Point p;
      f >> j >> p;
      skip_until_EOL(f); skip_comment_OFF(f);
      vertices[--j] = insert(p);
    }

  // read segments
  unsigned int number_of_segments;
  f >> number_of_segments;
  skip_until_EOL(f); skip_comment_OFF(f);
  for(unsigned int k = 0; k < number_of_segments; ++k)
    {
      unsigned int l, v1, v2;
      f >> l >> v1 >> v2;
      skip_until_EOL(f); skip_comment_OFF(f);
      insert_constraint(vertices[--v1], vertices[--v2]);
    }

  // read holes
  unsigned int number_of_holes;
  f >> number_of_holes;
  for(unsigned int m = 0; m < number_of_holes; ++m)
    {
      unsigned int n;
      Point p;
      f >> n >> p;
      skip_until_EOL(f); skip_comment_OFF(f);
      seeds.push_back(p);
    }
  if(dont_refine)
    init(seeds.begin(), seeds.end(), false);
  else
    refine(seeds.begin(), seeds.end(), false);
}

// HELPING FUNCTIONS

template <class Tr>
void Mesh_2<Tr>::
clear() 
{
  cluster_map.clear();
  edges_to_be_conformed.clear();
  bad_faces.clear();
  seeds.clear();
  Triangulation::clear();
}

// MESHING FUNCTIONS


//the mesh refine function 
template <class Tr>
inline
void Mesh_2<Tr>::
refine()
{
  std::list<Point> l;
  refine(l.begin(), l.end());
}

// REMESHING FUNCTIONS

// deprecated constructor
template <class Tr>
Mesh_2<Tr>::
Mesh_2(List_constraints& lc, const Geom_traits& gt)
  : Tr(gt), is_really_a_constrained_edge(*this),
    edges_to_be_conformed(is_really_a_constrained_edge)
{
  typename List_constraints::iterator lcit = lc.begin();
  for( ; lcit != lc.end(); ++lcit)
    {
      insert_constraint( (*lcit).first, (*lcit).second);
    }
  CGAL_triangulation_postcondition(is_valid());
}

template <class Tr>
void Mesh_2<Tr>::
set_geom_traits(const Geom_traits& gt)
{
  _gt = gt;
  bad_faces.clear();
  fill_facette_map();
}

// STEP BY STEP MESHING
template <class Tr>
inline
void Mesh_2<Tr>::
init()
{
  std::list<Point> l;
  init(l.end(), l.end());
}

template <class Tr>
inline
void Mesh_2<Tr>::
conform()
{
  while( !edges_to_be_conformed.empty() )
    {
      process_one_edge();	
    };
}

template <class Tr>
inline
bool Mesh_2<Tr>::
refine_step()
{
  if( !edges_to_be_conformed.empty() )
    process_one_edge();
  else
    if ( !bad_faces.empty() )
      process_one_face();
    else
      return false;
  return true;
}

// PRIVATE MEMBER FUNCTIONS

template <class Tr>
void Mesh_2<Tr>::
mark_convex_hull()
{
  for(All_faces_iterator fit=all_faces_begin();
      fit!=all_faces_end();
      ++fit)
    fit->set_marked(true);
  propagate_marks(infinite_face(), false);
}

template <class Tr>
void Mesh_2<Tr>::
propagate_marks(const Face_handle fh, bool mark)
{
  // std::queue only works with std::list on VC++6, and not with
  // std::deque, wichi is the default
  std::queue<Face_handle, std::list<Face_handle> > face_queue;
  fh->set_marked(mark);
  face_queue.push(fh);
  while( !face_queue.empty() )
    {
      Face_handle fh = face_queue.front();
      face_queue.pop();
      for(int i=0;i<3;i++)
	{
	  const Face_handle& nb = fh->neighbor(i);
	  if( !fh->is_constrained(i) && (mark != nb->is_marked()) )
	    {
	      nb->set_marked(mark);
	      face_queue.push(nb);
	    }
	}
    }
};

template <class Tr>
void Mesh_2<Tr>::
create_clusters()
{
  for(Finite_vertices_iterator vit = finite_vertices_begin();
      vit != finite_vertices_end();
      vit++)
    create_clusters_of_vertex(vit);
}

template <class Tr>
void Mesh_2<Tr>::
create_clusters_of_vertex(Vertex_handle v)
{
  Is_edge_constrained test(this, v);
  Constrained_vertex_circulator begin(incident_vertices(v),test);
  // This circulator represents all constrained edges around the
  // vertex v. An edge [v,v'] is represented by the vertex v'.

  if(begin == 0) return; // if there is only one vertex

  Constrained_vertex_circulator
    current(begin), next(begin), cluster_begin(begin);
  ++next; // next is always just after current.
  if(current == next) return;

  bool in_a_cluster = false;
  do
    {
      if(is_small_angle(current->point(), v->point(), next->point()))
	{
	  if(!in_a_cluster)
	    {
	      // at this point, current is the beginning of a cluster
	      in_a_cluster = true;
	      cluster_begin = current;
	    }
	}
      else
	if(in_a_cluster)
	  {
	    // at this point, current is the end of a cluster and
	    // cluster_begin is its beginning
 	    construct_cluster(v, cluster_begin, current);
	    in_a_cluster = false;
	  }
      ++next;
      ++current;
    } while( current!=begin );
  if(in_a_cluster)
    {
      Cluster c;
      if(get_cluster(v, begin, c, true)) 
	// get the cluster and erase it from the clusters map
	construct_cluster(v, cluster_begin, begin, c);
      else
	construct_cluster(v, cluster_begin, current);
    }
}

template <class Tr>
void Mesh_2<Tr>::
construct_cluster(Vertex_handle v,
		  Constrained_vertex_circulator begin,
		  const Constrained_vertex_circulator& end,
		  Cluster c)
{
  Compute_squared_distance_2 squared_distance = 
    geom_traits().compute_squared_distance_2_object();

  if(c.vertices.empty())
    {
      c.reduced = false;
      // c.rmin is not initialized because
      // reduced=false!
      c.minimum_squared_length = 
	squared_distance(v->point(), begin->point());
      Constrained_vertex_circulator second(begin);
      ++second;
      c.smallest_angle.first = begin;
      c.smallest_angle.second = second;
    }

  bool all_edges_in_cluster=false; // tell if all incident edges are
  // in the cluster
  if(begin==end)
    all_edges_in_cluster=true;

  const Point& vp = v->point();
  
  FT greatest_cosine = 
    squared_cosine_of_angle_times_4(c.smallest_angle.first->point(),
				    v->point(),
				    c.smallest_angle.second->point());

  Constrained_vertex_circulator next(begin);
  ++next;
  do
    {
      c.vertices[begin] = false;
      Squared_length l = squared_distance(vp,
					begin->point());
      c.minimum_squared_length = 
	std::min(l,c.minimum_squared_length);
      
      if(all_edges_in_cluster || begin!=end)
	{
	  FT cosine = 
	    squared_cosine_of_angle_times_4(begin->point(),
					    v->point(),
					    next->point());
	  if(cosine>greatest_cosine)
	    {
	      greatest_cosine = cosine;
	      c.smallest_angle.first = begin;
	      c.smallest_angle.second = next;
	    }
	}
    }
  while(next++,begin++!=end);
  cluster_map.insert(std::make_pair(v,c));
}

template <class Tr>
void Mesh_2<Tr>::
update_cluster(Cluster& c, Vertex_handle va,Vertex_handle vb,
	       Vertex_handle vm, bool reduction)
{
  Compute_squared_distance_2 squared_distance = 
    geom_traits().compute_squared_distance_2_object();
  c.vertices.erase(vb);
  c.vertices[vm] = reduction;
  
  if(vb==c.smallest_angle.first)
    c.smallest_angle.first = vm;
  if(vb==c.smallest_angle.second)
    c.smallest_angle.second = vm;

  FT l = squared_distance(va->point(),vm->point());
  if(l<c.minimum_squared_length)
    c.minimum_squared_length = l;

  if(!c.is_reduced())
    {
      typename Cluster::Vertices_map::iterator it = c.vertices.begin();
      while(it!=c.vertices.end() && c.is_reduced(it->first))
	++it; // TODO: use std::find and an object class
      if(it==c.vertices.end())
	c.reduced = true;
    }

  if(c.is_reduced())
    c.rmin = squared_distance(c.smallest_angle.first->point(),
			      c.smallest_angle.second->point())/FT(4);
  cluster_map.insert(std::make_pair(va,c));
}

// # used by refine_face and cut_cluster_edge
template <class Tr>
bool Mesh_2<Tr>::
get_cluster(Vertex_handle va, Vertex_handle vb, Cluster &c, bool erase)
{
  typedef typename Cluster_map::iterator Iterator;
  typedef std::pair<Iterator, Iterator> Range;
  Range range = cluster_map.equal_range(va);
  for(Iterator it = range.first; it != range.second; it++)
    {
      Cluster &cl = it->second;
      if(cl.vertices.find(vb)!=cl.vertices.end()) {
	c = it->second;
	if(erase)
	  cluster_map.erase(it);
	return true;
      }
    }
  return false;
}

template <class Tr>
void Mesh_2<Tr>::
fill_edge_queue()
{
  for(Finite_edges_iterator ei = finite_edges_begin();
      ei != finite_edges_end();
      ++ei)
    {
      Vertex_handle va = (*ei).first->vertex(cw((*ei).second));
      Vertex_handle vb = (*ei).first->vertex(ccw((*ei).second));
      if((*ei).first->is_constrained((*ei).second) && 
	 is_encroached(va, vb))
	{
	  edges_to_be_conformed.push_back(std::make_pair(va, vb));
	}
    }
}

//update the encroached segments list
// TODO: perhaps we should remove destroyed edges too
// TODO: rewrite this function one day
template <class Tr>
void Mesh_2<Tr>::
update_edges_to_be_conformed(Vertex_handle va, Vertex_handle vb, 
			     Vertex_handle vm)
{
  Face_circulator fc = incident_faces(vm), fcbegin(fc);

  do {
    for(int i = 0; i<3; i++) {
      Vertex_handle v1 = fc->vertex(cw(i));
      Vertex_handle v2 = fc->vertex(ccw(i));
      if( fc->is_constrained(i) && !is_infinite(v1) &&
	  !is_infinite(v2) && is_encroached(v1, v2) )
	edges_to_be_conformed.push_back(Constrained_edge(v1, v2));
    }
    ++fc;
  } while(fc != fcbegin);

  if(is_encroached(va, vm)) {
    edges_to_be_conformed.push_back(Constrained_edge(va, vm));
  }

  if(is_encroached(vb, vm)) {
    edges_to_be_conformed.push_back(Constrained_edge(vb, vm));
  }
}

template <class Tr>
inline
void Mesh_2<Tr>::
push_in_bad_faces(Face_handle fh)
{
  const Point
    & va = fh->vertex(0)->point(),
    & vb = fh->vertex(1)->point(),
    & vc = fh->vertex(2)->point();
  //  std::cerr << "push_in_bad_faces(" << va << ",  " << vb << ", " << vc << ")" << std::endl;  
  CGAL_assertion(fh->is_marked());
  bad_faces.insert(fh, squared_minimum_sine(fh));
}

template <class Tr>
inline
void Mesh_2<Tr>::
push_in_bad_faces(Vertex_handle va, Vertex_handle vb,
		  Vertex_handle vc)
{
  Face_handle fh;
  is_face(va, vb, vc, fh);
  push_in_bad_faces(fh);
}

//it is necessarry for process_facette_map
template <class Tr>
void Mesh_2<Tr>::
fill_facette_map()
{
  //  std::cerr << "bad_faces.size() = " << bad_faces.size() << std::endl;
 
  for(Finite_faces_iterator fit = finite_faces_begin();
      fit != finite_faces_end();
      ++fit)
    if( is_bad(fit) && fit->is_marked() )
      push_in_bad_faces(fit);
}

template <class Tr>
void Mesh_2<Tr>::
compute_new_bad_faces(Vertex_handle v)
{
  Face_circulator fc = v->incident_faces(), fcbegin(fc);
  do {
    if(!is_infinite(fc))
      if( is_bad(fc) && fc->is_marked() )
	push_in_bad_faces(fc);
    fc++;
  } while(fc!=fcbegin);
}

template <class Tr>
inline
void Mesh_2<Tr>::
process_one_edge()
{
  Constrained_edge ce = edges_to_be_conformed.front();
  edges_to_be_conformed.pop_front();

  refine_edge(ce.first, ce.second);
}

template <class Tr>
inline
void Mesh_2<Tr>::
process_one_face()
{
  //  std::cerr << "bad_faces.size() = " << bad_faces.size() << std::endl;
  Face_handle f = bad_faces.front()->second;
  bad_faces.pop_front();
  const Point
    & va = f->vertex(0)->point(),
    & vb = f->vertex(1)->point(),
    & vc = f->vertex(2)->point();
  //  std::cerr << "process_one_face(" << va << ",  " << vb << ", " << vc << ")" << std::endl;

  std::cerr << number_of_vertices() << std::endl;

  refine_face(f);
}

//this function split all the segments that are encroached
template <class Tr>
void Mesh_2<Tr>::
refine_edge(Vertex_handle va, Vertex_handle vb)
{
  Face_handle f;
  int i;
  is_edge(va, vb, f, i); // get the edge (f,i)
  CGAL_assertion(f->is_constrained(i));
  
  Cluster c,c2;

  if( get_cluster(va,vb,c,true) )
    if( get_cluster(vb,va,c2,true) )
      { // both ends are clusters
	Vertex_handle vm = insert_middle(f,i);
	update_cluster(c,va,vb,vm,false);
	update_cluster(c2,vb,va,vm,false);
      }
    else
      // va only is a cluster
      cut_cluster_edge(va,vb,c);
  else
    if( get_cluster(vb,va,c,true) )
      // vb only is a cluster
      cut_cluster_edge(vb,va,c);
    else
      // no cluster
      insert_middle(f,i);
};

 //split all the bad faces
template <class Tr>
void Mesh_2<Tr>::
refine_face(const Face_handle f)
{
  const Point& pc = circumcenter(f);

  List_of_edges zone_of_pc_boundary;
  List_of_face_handles zone_of_pc;

  // find conflicts around pc (starting from f as hint)
  get_conflicts_and_boundary(pc, 
			    std::back_inserter(zone_of_pc), 
			    std::back_inserter(zone_of_pc_boundary), 
			    f);
  // For the moment, we don't use the zone_of_pc.
  // It will be used when we will destroyed old bad faces in bad_faces

  bool split_the_face = true;
  bool keep_the_face_bad = false;

  for(typename List_of_edges::iterator it = zone_of_pc_boundary.begin();
      it!= zone_of_pc_boundary.end();
      it++)
    {
      const Face_handle& fh = it->first;
      const int& i = it->second;
      const Vertex_handle
	& va = fh->vertex(cw(i)),
	& vb = fh->vertex(ccw(i));
      if(fh->is_constrained(i) && is_encroached(va,vb,pc))
	{
	  split_the_face = false;
	  Cluster c,c2;
	  bool 
	    is_cluster_at_va = get_cluster(va,vb,c),
	    is_cluster_at_vb = get_cluster(vb,va,c2);
	  if( ( is_cluster_at_va &&  is_cluster_at_vb) || 
	      (!is_cluster_at_va && !is_cluster_at_vb) )
	    {
	      // two clusters or no cluster
	      edges_to_be_conformed.push_back(Constrained_edge(va,vb));
	      keep_the_face_bad = true;
	    }
	  else
	    {
	      // only one cluster: c or c2
	      if(is_cluster_at_vb)
		c = c2;
// What Shewchuk says:
// - If the cluster is not reduced (all segments don't have the same
// length as [va,vb]), then split the edge
// - Else, let rmin be the minimum insertion radius introduced by the
// potential split, let T be the triangle whose circumcenter
// encroaches [va,vb] and let rg be the length of the shortest edge
// of T. If rmin >= rg, then split the edge.

	      if( !c.is_reduced() || 
		  c.rmin >= shortest_edge_squared_length(f) )
		{
		  edges_to_be_conformed.push_back(Constrained_edge(va,vb));
		  keep_the_face_bad = true;
		}
	    }
	}
    }; // after here edges_to_be_conformed contains edges encroached by pc

  const Vertex_handle
    & va = f->vertex(0),
    & vb = f->vertex(1),
    & vc = f->vertex(2);

  if(split_the_face)
    {
//       int li;
//       Locate_type lt;
//       locate(pc,lt,li,f);
//       if(lt!=OUTSIDE_CONVEX_HULL)
      CGAL_assertion(f->is_marked());
      if(f->is_marked())
	split_face(f, pc);
    }
  else
    if(keep_the_face_bad)
      push_in_bad_faces(va, vb, vc);
}

template <class Tr>
bool Mesh_2<Tr>::
is_encroached(const Vertex_handle va, const Vertex_handle vb,
	      const Point& p) const
{
  Angle_2 angle = geom_traits().angle_2_object();

  return angle(va->point(), p, vb->point())==OBTUSE;
}

template <class Tr>
bool Mesh_2<Tr>::
is_encroached(const Vertex_handle va, const Vertex_handle vb) const
{
  Face_handle fh;
  int i;
  is_edge(va,vb,fh,i);

  const Point& candidat_1 = fh->vertex(i)->point();
  const Point& candidat_2 = fh->mirror_vertex(i)->point();

  return ( ( fh->is_marked() && 
	    is_encroached(va, vb, candidat_1) ) ||
	   ( fh->neighbor(i)->is_marked() && 
	     is_encroached(va, vb, candidat_2) )
	   );
}

// -> traits?
template <class Tr>
bool Mesh_2<Tr>::
is_small_angle(const Point& pleft,
	       const Point& pmiddle,
	       const Point& pright) const
{
  Angle_2 angle = geom_traits().angle_2_object();
  Orientation_2 orient = geom_traits().orientation_2_object();
  
  if( angle(pleft, pmiddle, pright)==OBTUSE )
    return false;
  if( orient(pmiddle,pleft,pright)==RIGHTTURN)
    return false;

  FT cos_alpha = squared_cosine_of_angle_times_4(pleft, pmiddle,
						 pright);

  if(cos_alpha > 1)
    {
      return true; //the same cluster
    }
  else
    {
      return false; //another cluster
    }
}

template <class Tr>
void Mesh_2<Tr>::
cut_cluster_edge(Vertex_handle va, Vertex_handle vb, Cluster& c)
{
  Construct_vector_2 vector =
    geom_traits().construct_vector_2_object();
  Construct_scaled_vector_2 scaled_vector =
    geom_traits().construct_scaled_vector_2_object();
  Compute_squared_distance_2 squared_distance =
    geom_traits().compute_squared_distance_2_object();
  Construct_midpoint_2 midpoint = 
    geom_traits().construct_midpoint_2_object();
  Construct_translated_point_2 translate =
    geom_traits().construct_translated_point_2_object();

  Vertex_handle vc;

  if(c.is_reduced(vb))
    {
      Face_handle fh;
      int i;
      is_edge(va,vb,fh,i);
      vc = insert_middle(fh,i);
    }
  else
    {
      const Point
	& a = va->point(),
	& b = vb->point(),
	& m = midpoint(a, b);



      Vector_2 v = vector(a,m);
      v = scaled_vector(v,CGAL_NTS sqrt(c.minimum_squared_length /
				      squared_distance(a,b)));

      Point i = translate(a,v), i2(i);

      do {
	i = translate(a,v);
	v = scaled_vector(v,FT(2));
	i2 = translate(a,v);
      }	while(squared_distance(a,i2) <= squared_distance(a,m));
      if( squared_distance(i,m) > squared_distance(m,i2) )
	i = i2;
      //here i is the best point for splitting
      Face_handle fh;
      int index;
      is_edge(va,vb,fh,index);

      vc = insert_in_the_edge(fh, index, i);
    }
  update_edges_to_be_conformed(va, vb, vc);
  compute_new_bad_faces(vc);
  update_cluster(c, va, vb, vc);
}

template <class Tr>
typename Mesh_2<Tr>::Vertex_handle
Mesh_2<Tr>::
insert_middle(Face_handle f, int i)
{
  Construct_midpoint_2
    midpoint = geom_traits().construct_midpoint_2_object();

  const Vertex_handle
    & va = f->vertex(cw(i)),
    & vb = f->vertex(ccw(i));

  const Point& mp = midpoint(va->point(), vb->point());

  Vertex_handle vm = insert_in_the_edge(f, i, mp);

  update_edges_to_be_conformed(va, vb, vm);
  return vm;
}
// # used by refine_face
template <class Tr>
inline
void Mesh_2<Tr>::
split_face(const Face_handle& f, const Point& circum_center)
{
  bool marked = f->is_marked();

  List_of_face_handles zone_of_cc;
  List_of_edges zone_of_cc_boundary;

  get_conflicts_and_boundary(circum_center, 
			     std::back_inserter(zone_of_cc),
			     std::back_inserter(zone_of_cc_boundary),
			     f);
  for(typename List_of_face_handles::iterator fh_it = zone_of_cc.begin();
      fh_it != zone_of_cc.end();
      ++fh_it)
    bad_faces.erase(*fh_it);

  // insert the point in the triangulation with star_hole
  Vertex_handle v = star_hole(circum_center,
			      zone_of_cc_boundary.begin(),
			      zone_of_cc_boundary.end(),
			      zone_of_cc.begin(),
			      zone_of_cc.end());

  Face_circulator fc = incident_faces(v), fcbegin(fc);
  do {
    fc->set_marked(marked);
  } while (++fc != fcbegin);

  compute_new_bad_faces(v);
}

template <class Tr>
inline 
typename Mesh_2<Tr>::Vertex_handle
Mesh_2<Tr>::
insert_in_the_edge(Face_handle fh, int edge_index, const Point& p)
  // insert the point p in the edge (fh, edge_index). It updates seeds 
  // too.
{
  const Vertex_handle
    & va = fh->vertex(cw(edge_index)),
    & vb = fh->vertex(ccw(edge_index));

  bool 
    mark_at_right = fh->is_marked(),
    mark_at_left = fh->neighbor(edge_index)->is_marked();

  List_of_face_handles zone_of_p;

  // deconstrain the edge before finding the conflicts
  //  fh->set_constraint(edge_index,false);
  //  fh->neighbor(edge_index)->set_constraint(fh->mirror_index(edge_index),false);

  get_conflicts_and_boundary(p, 
			     std::back_inserter(zone_of_p), 
			     Emptyset_iterator(), 
			     EDGE, fh, edge_index);

  // reconstrain the edge
  //  fh->set_constraint(edge_index,true);
  //  fh->neighbor(edge_index)->set_constraint(fh->mirror_index(edge_index),true);
  
  //  std::cerr << "insert_in_the_edge: conflict_size=" << zone_of_p.size() << std::endl;
  for(typename List_of_face_handles::iterator fh_it = zone_of_p.begin();
      fh_it != zone_of_p.end();
      ++fh_it)
    bad_faces.erase(*fh_it);

  Vertex_handle vp = insert(p, EDGE, fh, edge_index);
  // TODO, WARNING: this is not robust!
  // We should deconstrained the constrained edge, insert the two
  // subconstraints and re-constrain them

  //  CGAL_assertion(is_bad_faces_valid());

  is_edge(va, vp, fh, edge_index); 
  // set fh to the face at the right of [va,vb]

  Face_circulator fc = incident_faces(vp, fh), fcbegin(fc);
  // circulators are counter-clockwise, so we start at the right of
  // [va,vb]
  do {
    if( !is_infinite(fc) )
      fc->set_marked(mark_at_right);
    ++fc;
  } while ( fc->vertex(ccw(fc->index(vp))) != vb );
  // we are now at the left of [va,vb]
  do {
    if( !is_infinite(fc) )
      fc->set_marked(mark_at_left);
    ++fc;
  } while ( fc != fcbegin );

  compute_new_bad_faces(vp);
  return vp;
}

// # used by: is_small_angle, create_clusters_of_vertex
// # compute 4 times the square of the cosine of the angle (ab,ac)
// # WARNING, TODO: this is not exact with doubles and can lead to crashes!!
template <class Tr>
typename Mesh_2<Tr>::FT Mesh_2<Tr>::
squared_cosine_of_angle_times_4(const Point& pb, const Point& pa,
				const Point& pc) const
{
  Compute_squared_distance_2 squared_distance = 
    geom_traits().compute_squared_distance_2_object();

  const FT
    a = squared_distance(pb, pc),
    b = squared_distance(pa, pb),
    c = squared_distance(pa, pc);

  const FT num = a-(b+c);

  return (num*num)/(b*c);
}

// ->traits?
//the shortest edge that are in a triangle
// # used by: refine_face, squared_minimum_sine
template <class Tr>
typename Mesh_2<Tr>::FT Mesh_2<Tr>::
shortest_edge_squared_length(Face_handle f)
{
  Compute_squared_distance_2 squared_distance = 
    geom_traits().compute_squared_distance_2_object();
  const Point 
    & pa = (f->vertex(0))->point(),
    & pb = (f->vertex(1))->point(),
    & pc = (f->vertex(2))->point();
  FT a, b, c;
  a = squared_distance(pb, pc);
  b = squared_distance(pc, pa);
  c = squared_distance(pa, pb);
  return (min(a, min(b, c)));
}

template <class Tr>
bool Mesh_2<Tr>::
is_bad_faces_valid()
{
  std::cerr << "is_bad_faces_valid()" << std::endl;
  typedef std::list<std::pair<double, Face_handle> > Bad_faces_list;
  
  bool result = true;

  Bad_faces_list bad_faces_list;

  while(!bad_faces.empty())
    {
      double d = bad_faces.front()->first;
      Face_handle fh = bad_faces.front()->second;
      bad_faces.pop_front();
      
      bad_faces_list.push_back(std::make_pair(d, fh));

      const Vertex_handle
	& va = fh->vertex(0),
	& vb = fh->vertex(1),
	& vc = fh->vertex(2);

      Face_handle fh2;
      if( ! ( is_face(va, vb, vc, fh2) && (fh == fh2) && 
	  fh->is_marked() && is_bad(fh) ) )
	{
	  result = false;
	  std::cerr << "Invalid bad face: (" << va->point() << ", "
		    << vb->point() << ", " << vc->point() << ")" << std::endl;
	}
    }

  for(typename Bad_faces_list::iterator it = bad_faces_list.begin();
      it != bad_faces_list.end();
      ++it)
    bad_faces.insert(it->second, it->first);
  
  return result;
}

CGAL_END_NAMESPACE


#endif
