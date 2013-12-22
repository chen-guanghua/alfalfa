#ifndef TWOD_HH
#define TWOD_HH

#include <cassert>
#include <vector>

#include "optional.hh"

/* simple two-dimensional container */
template< class T >
class TwoDBase
{
public:
  virtual unsigned int width( void ) const = 0;
  virtual unsigned int height( void ) const = 0;

  virtual T & at( const unsigned int column, const unsigned int row ) = 0;

  template <class lambda>
  void forall( const lambda & f )
  {
    for ( unsigned int row = 0; row < height(); row++ ) {
      for ( unsigned int column = 0; column < width(); column++ ) {
	f( at( column, row ) );
      }
    }
  }

  template <class lambda>
  void forall_ij( const lambda & f )
  {
    for ( unsigned int row = 0; row < height(); row++ ) {
      for ( unsigned int column = 0; column < width(); column++ ) {
	f( at( column, row ), column, row );
      }
    }
  }
};

template< class T >
class TwoDSubRange;

template< class T >
class TwoD : public TwoDBase< T >
{
private:
  unsigned int width_, height_;
  std::vector< T > storage_;

public:
  struct Context
  {
    const unsigned int column, row;
    const Optional< const T * > left, above_left, above, above_right;

    Context( const unsigned int s_column, const unsigned int s_row, const TwoD & self )
      : column( s_column ), row( s_row ),
	left(        self.maybe_at( column - 1, row ) ),
	above_left(  self.maybe_at( column - 1, row - 1 ) ),
	above(       self.maybe_at( column,     row - 1 ) ),
	above_right( self.maybe_at( column + 1, row - 1 ) )
    {}
  };

  template< typename... Targs >
  TwoD( const unsigned int width, const unsigned int height, Targs&&... Fargs )
    : width_( width ), height_( height ), storage_()
  {
    assert( width > 0 );
    assert( height > 0 );

    storage_.reserve( width * height );

    /* we want to construct each member separately */
    for ( unsigned int row = 0; row < height; row++ ) {
      for ( unsigned int column = 0; column < width; column++ ) {
	const Context c( column, row, *this );
	storage_.emplace_back( c, Fargs... );
      }
    }
  }

  T & at( const unsigned int column, const unsigned int row ) override
  {
    if ( column >= width_ or row >= height_ ) {
      throw std::out_of_range( "attempted to read outside of TwoD structure" );
    }
    return storage_.at( row * width_ + column );
  }

  Optional< const T * > maybe_at( const unsigned int column, const unsigned int row ) const
  {
    if ( column < width_ and row < height_ ) {
      return &storage_.at( row * width_ + column );
    } else {
      return Optional< const T * >();
    }
  }

  unsigned int width( void ) const override { return width_; }
  unsigned int height( void ) const override { return height_; }

  /* forbid copying */
  TwoD( const TwoD & other ) = delete;
  TwoD & operator=( const TwoD & other ) = delete;

  /* allow moving */
  TwoD( TwoD && other )
    : width_( other.width_ ),
      height_( other.height_ ),
      storage_( move( other.storage_ ) )
  {}
};

template< class T >
class TwoDSubRange : public TwoDBase< T >
{
private:
  TwoD< T > & master_;

  unsigned int column_, row_;
  unsigned int width_, height_;

public:
  TwoDSubRange( TwoD< T > & master,
		const unsigned int column,
		const unsigned int row,
		const unsigned int width,
		const unsigned int height )
    : master_( master ), column_( column ), row_( row ), width_( width ), height_( height )
  {
    assert( column_ + width <= master_.width() );
    assert( row_ + height <= master_.height() );
  }

  TwoDSubRange( TwoDSubRange< T > & parent,
		const unsigned int column,
		const unsigned int row,
		const unsigned int width,
		const unsigned int height )
    : master_( parent.master_ ),
      column_( column + parent.column_ ),
      row_( row + parent.row_ ),
      width_( width ),
      height_( height )
  {
    assert( column_ + width_ <= master_.width() );
    assert( row_ + height_ <= master_.height() );
  }

  T & at( const unsigned int column, const unsigned int row ) override
  {
    if ( column >= width_ or row >= height_ ) {
      throw std::out_of_range( "attempted to read outside of TwoDSubRange" );
    }
    return master_.at( column_ + column, row_ + row );
  }

  unsigned int width( void ) const override { return width_; }
  unsigned int height( void ) const override { return height_; }
};

#endif /* TWOD_HH */
