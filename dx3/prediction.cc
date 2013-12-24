#include <cmath>

#include "macroblock_header.hh"
#include "raster.hh"

template <unsigned int size>
Raster::Block<size>::Block( const typename TwoD< Block >::Context & c,
			    TwoD< Component > & raster_component )
  : contents( raster_component, size * c.column, size * c.row ),
    context( c ),
    predictors( context )
{}

Raster::Macroblock::Macroblock( const TwoD< Macroblock >::Context & c, Raster & raster )
  : Y( raster.Y_bigblocks_.at( c.column, c.row ) ),
    U( raster.U_bigblocks_.at( c.column, c.row ) ),
    V( raster.V_bigblocks_.at( c.column, c.row ) ),
    Y_sub( raster.Y_subblocks_, 4 * c.column, 4 * c.row ),
    U_sub( raster.U_subblocks_, 2 * c.column, 2 * c.row ),
    V_sub( raster.V_subblocks_, 2 * c.column, 2 * c.row )
{
  /* adjust "extra pixels" for rightmost Y subblocks in macroblock (other than the top one) */
  for ( unsigned int row = 1; row < 4; row++ ) {
    Y_sub.at( 3, row ).predictors.above_right_bottom_row.set( Y_sub.at( 3, 0 ).predictors.above_right_bottom_row );
  }
}

Raster::Raster( const unsigned int macroblock_width, const unsigned int macroblock_height,
		const unsigned int display_width, const unsigned int display_height )
  : width_( macroblock_width * 16 ), height_( macroblock_height * 16 ),
    display_width_( display_width ), display_height_( display_height )
{}

template <unsigned int size>
const typename Raster::Block<size>::Predictors::Row & Raster::Block<size>::Predictors::row127( void )
{
  static TwoD< Component > storage( size, 1, 127 );
  static const Row row( storage, 0, 0 );
  return row;
}

template <unsigned int size>
const typename Raster::Block<size>::Predictors::Column & Raster::Block<size>::Predictors::col129( void )
{
  static TwoD< Component > storage( 1, size, 129 );
  static const Column col( storage, 0, 0 );
  return col;
}

template <unsigned int size>
Raster::Block<size>::Predictors::Predictors( const typename TwoD< Block >::Context & context )
  : above_row( context.above.initialized()
	       ? context.above.get()->contents.row( size - 1 )
	       : row127() ),
    left_column( context.left.initialized()
		 ? context.left.get()->contents.column( size - 1 )
		 : col129() ),
    above_left( context.above_left.initialized()
		? context.above_left.get()->at( size - 1, size - 1 )
		: ( context.above.initialized()
		    ? col129().at( 0, 0 )
		    : row127().at( 0, 0 ) ) ),
  above_right_bottom_row( context.above_right.initialized()
			  ? context.above_right.get()->contents.row( size - 1 )
			  : row127() ),
  above_bottom_right_pixel( context.above.initialized()
			    ? context.above.get()->at( size - 1, size - 1 )
			    : row127().at( 0, 0 ) ),
  use_row( context.above_right.initialized() )
{}

template <unsigned int size>
Component Raster::Block<size>::Predictors::above_right( const unsigned int column ) const
{
  return use_row ? above_right_bottom_row.at( column, 0 ) : above_bottom_right_pixel;
}

template <unsigned int size>
Component Raster::Block<size>::Predictors::above( const int column ) const
{
  assert( column >= -1 and column < int( size * 2 ) );
  if ( column == -1 ) return above_left;
  if ( 0 <= column and column < size ) return above_row.at( column, 0 );
  return above_right( column - size );
}

template <unsigned int size>
Component Raster::Block<size>::Predictors::left( const int row ) const
{
  assert( row >= -1 and row < int( size ) );
  if ( row == -1 ) return above_left;
  return left_column.at( 0, row );
}

template <unsigned int size>
Component Raster::Block<size>::Predictors::east( const int num ) const
{
  assert( 0 <= num and num <= size * 2 );
  if ( num <= 4 ) { return left( 3 - num ); }
  return above( num - 5 );
}

template <unsigned int size>
void Raster::Block<size>::true_motion_predict( void )
{
  contents.forall_ij( [&] ( Component & b, unsigned int column, unsigned int row )
		      { b.clamp( predictors.left_column.at( 0, row )
				 + predictors.above_row.at( column, 0 )
				 - predictors.above_left ); } );
}

template <unsigned int size>
void Raster::Block<size>::horizontal_predict( void )
{
  for ( unsigned int row = 0; row < size; row++ ) {
    contents.row( row ).fill( predictors.left_column.at( 0, row ) );
  }
}

template <unsigned int size>
void Raster::Block<size>::vertical_predict( void )
{
  for ( unsigned int column = 0; column < size; column++ ) {
    contents.column( column ).fill( predictors.above_row.at( column, 0 ) );
  }
}

template <unsigned int size>
void Raster::Block<size>::dc_predict_simple( void )
{
  static_assert( size == 4 or size == 8 or size == 16, "invalid Block size" );
  static constexpr uint8_t log2size = size == 4 ? 2 : size == 8 ? 3 : size == 16 ? 4 : 0;

  contents.fill( ((predictors.above_row.sum(int16_t())
		   + predictors.left_column.sum(int16_t())) + (1 << log2size))
		 >> (log2size+1) );
}

template <unsigned int size>
void Raster::Block<size>::dc_predict( void )
{
  if ( context.above.initialized() and context.left.initialized() ) {
    return dc_predict_simple();
  }

  Component value = 128;
  static_assert( size == 4 or size == 8 or size == 16, "invalid Block size" );
  static constexpr uint8_t log2size = size == 4 ? 2 : size == 8 ? 3 : size == 16 ? 4 : 0;

  if ( context.above.initialized() ) {
    value = (predictors.above_row.sum(int16_t()) + (1 << (log2size-1))) >> log2size;
  } else if ( context.left.initialized() ) {
    value = (predictors.left_column.sum(int16_t()) + (1 << (log2size-1))) >> log2size;
  }

  contents.fill( value );
}

template <>
template <>
void Raster::Block8::intra_predict( const intra_mbmode uv_mode )
{
  /* Chroma prediction */

  switch ( uv_mode ) {
  case DC_PRED: dc_predict(); break;
  case V_PRED:  vertical_predict();  break;
  case H_PRED:  horizontal_predict();  break;
  case TM_PRED: true_motion_predict(); break;
  case B_PRED: assert( false ); break; /* tree decoder for uv_mode can't produce this */
  }
}

template <>
template <>
void Raster::Block16::intra_predict( const intra_mbmode uv_mode )
{
  /* Y prediction for whole macroblock */

  switch ( uv_mode ) {
  case DC_PRED: dc_predict(); break;
  case V_PRED:  vertical_predict();  break;
  case H_PRED:  horizontal_predict();  break;
  case TM_PRED: true_motion_predict(); break;
  case B_PRED: assert( false ); break; /* need to predict and transform subblocks independently */
  }
}

uint8_t avg3( const uint8_t x, const uint8_t y, const uint8_t z )
{
  return (x + 2 * y + z + 2) >> 2;
}

uint8_t avg2( const uint8_t x, const uint8_t y )
{
  return (x + y + 1) >> 1;
}

template <>
void Raster::Block4::vertical_smoothed_predict( void )
{
  contents.column( 0 ).fill( avg3( above( -1 ), above( 0 ), above( 1 ) ) );
  contents.column( 1 ).fill( avg3( above( 0 ),  above( 1 ), above( 2 ) ) );
  contents.column( 2 ).fill( avg3( above( 1 ),  above( 2 ), above( 3 ) ) );
  contents.column( 3 ).fill( avg3( above( 2 ),  above( 3 ), above( 4 ) ) );
}

template <>
void Raster::Block4::horizontal_smoothed_predict( void )
{
  contents.row( 0 ).fill( avg3( left( -1 ), left( 0 ), left( 1 ) ) );
  contents.row( 1 ).fill( avg3( left( 0 ),  left( 1 ), left( 2 ) ) );
  contents.row( 2 ).fill( avg3( left( 1 ),  left( 2 ), left( 3 ) ) );
  contents.row( 3 ).fill( avg3( left( 2 ),  left( 3 ), left( 3 ) ) );
  /* last line is special because we can't use left( 4 ) yet */
}

template <>
void Raster::Block4::left_down_predict( void )
{
  at( 0, 0 ) =                                        avg3( above( 0 ), above( 1 ), above( 2 ) );
  at( 1, 0 ) = at( 0, 1 ) =                           avg3( above( 1 ), above( 2 ), above( 3 ) );
  at( 2, 0 ) = at( 1, 1 ) = at( 0, 2 ) =              avg3( above( 2 ), above( 3 ), above( 4 ) );
  at( 3, 0 ) = at( 2, 1 ) = at( 1, 2 ) = at( 0, 3 ) = avg3( above( 3 ), above( 4 ), above( 5 ) );
  at( 3, 1 ) = at( 2, 2 ) = at( 1, 3 ) =              avg3( above( 4 ), above( 5 ), above( 6 ) );
  at( 3, 2 ) = at( 2, 3 ) =                           avg3( above( 5 ), above( 6 ), above( 7 ) );
  at( 3, 3 ) =                                        avg3( above( 6 ), above( 7 ), above( 7 ) );
  /* last line is special because we don't use above( 8 ) */
}

template <>
void Raster::Block4::right_down_predict( void )
{
  at( 0, 3 ) =                                        avg3( east( 0 ), east( 1 ), east( 2 ) );
  at( 1, 3 ) = at( 0, 2 ) =                           avg3( east( 1 ), east( 2 ), east( 3 ) );
  at( 2, 3 ) = at( 1, 2 ) = at( 0, 1 ) =              avg3( east( 2 ), east( 3 ), east( 4 ) );
  at( 3, 3 ) = at( 2, 2 ) = at( 1, 1 ) = at( 0, 0 ) = avg3( east( 3 ), east( 4 ), east( 5 ) );
  at( 3, 2 ) = at( 2, 1 ) = at( 1, 0 ) =              avg3( east( 4 ), east( 5 ), east( 6 ) );
  at( 3, 1 ) = at( 2, 0 ) =                           avg3( east( 5 ), east( 6 ), east( 7 ) );
  at( 3, 0 ) =                                        avg3( east( 6 ), east( 7 ), east( 8 ) );
}

template <>
void Raster::Block4::vertical_right_predict( void )
{
  at( 0, 3 ) =                                        avg3( east( 1 ), east( 2 ), east( 3 ) );
  at( 0, 2 ) =                                        avg3( east( 2 ), east( 3 ), east( 4 ) );
  at( 1, 3 ) = at( 0, 1 ) =                           avg3( east( 3 ), east( 4 ), east( 5 ) );
  at( 1, 2 ) = at( 0, 0 ) =                           avg2( east( 4 ), east( 5 ) );
  at( 2, 3 ) = at( 1, 1 ) =                           avg3( east( 4 ), east( 5 ), east( 6 ) );
  at( 2, 2 ) = at( 1, 0 ) =                           avg2( east( 5 ), east( 6 ) );
  at( 3, 3 ) = at( 2, 1 ) =                           avg3( east( 5 ), east( 6 ), east( 7 ) );
  at( 3, 2 ) = at( 2, 0 ) =                           avg2( east( 6 ), east( 7 ) );
  at( 3, 1 ) =                                        avg3( east( 6 ), east( 7 ), east( 8 ) );
  at( 3, 0 ) =                                        avg2( east( 7 ), east( 8 ) );
}

template <>
void Raster::Block4::vertical_left_predict( void )
{
  at( 0, 0 ) =                                        avg2( above( 0 ), above( 1 ) );
  at( 0, 1 ) =                                        avg3( above( 0 ), above( 1 ), above( 2 ) );
  at( 0, 2 ) = at( 1, 0 ) =                           avg2( above( 1 ), above( 2 ) );
  at( 1, 1 ) = at( 0, 3 ) =                           avg3( above( 1 ), above( 2 ), above( 3 ) );
  at( 1, 2 ) = at( 2, 0 ) =                           avg2( above( 2 ), above( 3 ) );
  at( 1, 3 ) = at( 2, 1 ) =                           avg3( above( 2 ), above( 3 ), above( 4 ) );
  at( 2, 2 ) = at( 3, 0 ) =                           avg2( above( 3 ), above( 4 ) );
  at( 2, 3 ) = at( 3, 1 ) =                           avg3( above( 3 ), above( 4 ), above( 5 ) );
  at( 3, 2 ) =                                        avg3( above( 4 ), above( 5 ), above( 6 ) );
  at( 3, 3 ) =                                        avg3( above( 5 ), above( 6 ), above( 7 ) );
}

template <>
void Raster::Block4::horizontal_down_predict( void )
{
  at( 0, 3 ) =                                        avg2( east( 0 ), east( 1 ) );
  at( 1, 3 ) =                                        avg3( east( 0 ), east( 1 ), east( 2 ) );
  at( 0, 2 ) = at( 2, 3 ) =                           avg2( east( 1 ), east( 2 ) );
  at( 1, 2 ) = at( 3, 3 ) =                           avg3( east( 1 ), east( 2 ), east( 3 ) );
  at( 2, 2 ) = at( 0, 1 ) =                           avg2( east( 2 ), east( 3 ) );
  at( 3, 2 ) = at( 1, 1 ) =                           avg3( east( 2 ), east( 3 ), east( 4 ) );
  at( 2, 1 ) = at( 0, 0 ) =                           avg2( east( 3 ), east( 4 ) );
  at( 3, 1 ) = at( 1, 0 ) =                           avg3( east( 3 ), east( 4 ), east( 5 ) );
  at( 2, 0 ) =                                        avg3( east( 4 ), east( 5 ), east( 6 ) );
  at( 3, 0 ) =                                        avg3( east( 5 ), east( 6 ), east( 7 ) );
}

template <>
void Raster::Block4::horizontal_up_predict( void )
{
  at( 0, 0 ) =                                        avg2( left( 0 ), left( 1 ) );
  at( 1, 0 ) =                                        avg3( left( 0 ), left( 1 ), left( 2 ) );
  at( 2, 0 ) = at( 0, 1 ) =                           avg2( left( 1 ), left( 2 ) );
  at( 3, 0 ) = at( 1, 1 ) =                           avg3( left( 1 ), left( 2 ), left( 3 ) );
  at( 2, 1 ) = at( 0, 2 ) =                           avg2( left( 2 ), left( 3 ) );
  at( 3, 1 ) = at( 1, 2 ) =                           avg3( left( 2 ), left( 3 ), left( 3 ) );
  at( 2, 2 ) = at( 3, 2 ) = at( 0, 3 )
    = at( 1, 3 ) = at( 2, 3 ) = at( 3, 3 ) =          left( 3 );
}

template <>
template <>
void Raster::Block4::intra_predict( const intra_bmode b_mode )
{
  /* Luma prediction */

  switch ( b_mode ) {
  case B_DC_PRED: dc_predict_simple(); break; 
  case B_TM_PRED: true_motion_predict(); break;
  case B_VE_PRED: vertical_smoothed_predict(); break;
  case B_HE_PRED: horizontal_smoothed_predict(); break;
  case B_LD_PRED: left_down_predict(); break;
  case B_RD_PRED: right_down_predict(); break;
  case B_VR_PRED: vertical_right_predict(); break;
  case B_VL_PRED: vertical_left_predict(); break;
  case B_HD_PRED: horizontal_down_predict(); break;
  case B_HU_PRED: horizontal_up_predict(); break;
  }
}
