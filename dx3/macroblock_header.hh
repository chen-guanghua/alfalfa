#ifndef MB_RECORDS_HH
#define MB_RECORDS_HH

#include "vp8_header_structures.hh"
#include "frame_header.hh"
#include "2d.hh"
#include "block.hh"
#include "raster.hh"

#include "tree.cc"

class KeyFrameMacroblockHeader
{
private:
  Optional< Tree< uint8_t, 4, segment_id_tree > > segment_id_;
  Optional< Bool > mb_skip_coeff_;

  Y2Block & Y2_;
  TwoDSubRange< YBlock > Y_;
  TwoDSubRange< UVBlock > U_, V_;

public:
  KeyFrameMacroblockHeader( const TwoD< KeyFrameMacroblockHeader >::Context & c,
			    BoolDecoder & data,
			    const KeyFrameHeader & key_frame_header,
			    const KeyFrameHeader::DerivedQuantities & probability_tables,
			    TwoD< Y2Block > & frame_Y2,
			    TwoD< YBlock > & frame_Y,
			    TwoD< UVBlock > & frame_U,
			    TwoD< UVBlock > & frame_V );

  void parse_tokens( BoolDecoder & data,
		     const KeyFrameHeader::DerivedQuantities & probability_tables );

  void dequantize( const KeyFrameHeader::DerivedQuantities & derived );

  void inverse_transform( Raster::Macroblock & raster );
};

#endif /* MB_RECORDS_HH */
