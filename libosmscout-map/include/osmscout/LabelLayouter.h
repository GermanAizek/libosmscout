#ifndef OSMSCOUT_MAP_LABELLAYOUTER_H
#define OSMSCOUT_MAP_LABELLAYOUTER_H

/*
  This source is part of the libosmscout-map library
  Copyright (C) 2016  Tim Teulings

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
*/

#include <memory>
#include <set>

#include <osmscout/MapImportExport.h>

#include <osmscout/StyleConfig.h>
#include <osmscout/SimplifiedPath.h>

namespace osmscout {

  class IntRectangle {
  public:
    int x;
    int y;
    int width;
    int height;
  };

  class OSMSCOUT_MAP_API LabelData
  {
  public:
    size_t                   id;       //!< Id of this label, multiple labels with the same id do not intersect with each other
    size_t                   priority; //!< Priority of the entry

    double                   alpha;    //!< Alpha value of the label
    double                   fontSize; //!< Font size to be used
    double                   proposedWidth;
    LabelStyleRef            style;    //!< Style for drawing
    std::string              text;     //!< The label text

  public:
    LabelData();
    virtual ~LabelData();
  };


  template<class NativeGlyph>
  class Glyph {
  public:
    NativeGlyph glyph;
    osmscout::Vertex2D position;
    double angle{0}; //!< clock-wise rotation in radians

    osmscout::Vertex2D tl{0,0};
    osmscout::Vertex2D tr{0,0};
    osmscout::Vertex2D br{0,0};
    osmscout::Vertex2D bl{0,0};

    osmscout::Vertex2D trPosition{0,0}; //!< top-left position after rotation
    double trWidth{0};                  //!< width after rotation
    double trHeight{0};                 //!< height after rotation
  };

  template<class NativeGlyph, class NativeLabel>
  class Label
  {
  public:
    NativeLabel             label;

    double                  width;
    double                  height;

    double                  fontSize; //!< Font size to be used
    osmscout::LabelStyleRef style;    //!< Style for drawing
    std::string             text;     //!< The label text

    std::vector<Glyph<NativeGlyph>> toGlyphs() const;
  };

  template<class NativeGlyph, class NativeLabel>
  class LabelInstance
  {
  public:
    size_t                  id;       //!< Id of this label, multiple labels with the same id do not intersect with each other
    size_t                  priority; //!< Priority of the entry

    double                  x;        //!< Coordinate of the left, top edge of the text
    double                  y;        //!< Coordinate of the left, top edge of the text
    double                  alpha;    //!< Alpha value of the label

    Label<NativeGlyph, NativeLabel>
        label;

  };

  template<class NativeGlyph>
  class ContourLabel
  {
  public:
    size_t priority;
    std::vector<Glyph<NativeGlyph>> glyphs;
  };

  namespace {
    class Mask
    {
    public:
      Mask(size_t rowSize) : d(rowSize)
      {
        //std::cout << "create " << this << std::endl;
      };

      Mask(const Mask &m) :
          d(m.d), cellFrom(m.cellFrom), cellTo(m.cellTo), rowFrom(m.rowFrom), rowTo(m.rowTo)
      {
        //std::cout << "create(2) " << this << std::endl;
      };

      ~Mask()
      {
        //std::cout << "delete " << this << std::endl;
      }

      Mask(Mask &&m) = delete;
      Mask &operator=(const Mask &m) = delete;
      Mask &operator=(Mask &&m) = delete;

      void prepare(const IntRectangle &rect);

      inline int64_t size() const
      { return d.size(); };

      std::vector<uint64_t> d;

      int cellFrom{0};
      int cellTo{0};
      int rowFrom{0};
      int rowTo{0};
    };

    void Mask::prepare(const IntRectangle &rect)
    {
      cellFrom = rect.x / 64;
      int cellFromBit = rect.x % 64;
      cellTo = (rect.x + rect.width) / 64;
      int cellToBit = (rect.x + rect.width) % 64;
      rowFrom = rect.y;
      rowTo = rect.y + rect.height;

      if (cellFromBit<0){
        cellFrom--;
        cellFromBit=64+cellFromBit;
      }
      if (cellToBit<0){
        cellTo--;
        cellToBit=64+cellToBit;
      }

      uint64_t mask = ~0;
      for (int c = std::max(0, cellFrom); c <= std::min((int) d.size() - 1, cellTo); c++) {
        d[c] = mask;
      }
      if (cellFrom >= 0 && cellFrom < size()) {
        d[cellFrom] = d[cellFrom] >> cellFromBit;
      }
      if (cellTo >= 0 && cellTo < size()) {
        d[cellTo] = d[cellTo] << (64 - cellToBit);
      }
    }
  }

  template <class NativeGlyph>
  osmscout::Vertex2D glyphTopLeft(const NativeGlyph &glyph);

  template <class NativeGlyph>
  double glyphWidth(const NativeGlyph &glyph);

  template <class NativeGlyph>
  double glyphHeight(const NativeGlyph &glyph);

  template <class NativeGlyph, class NativeLabel, class TextLayouter>
  class OSMSCOUT_MAP_API LabelLayouter
  {

  public:
    using ContourLabelType = ContourLabel<NativeGlyph>;
    using LabelType = Label<NativeGlyph, NativeLabel>;
    using LabelInstanceType = LabelInstance<NativeGlyph, NativeLabel>;

  public:
    LabelLayouter(TextLayouter *textLayouter):
        textLayouter(textLayouter)
    {};

    void reset()
    {
      contourLabelInstances.clear();
      labelInstances.clear();
    }

    inline bool checkLabelCollision(const std::vector<uint64_t> &canvas,
                                    const Mask &mask,
                                    int64_t viewportHeight
    )
    {
      bool collision=false;
      for (int r=std::max(0,mask.rowFrom); !collision && r<=std::min((int)viewportHeight-1, mask.rowTo); r++){
        for (int c=std::max(0,mask.cellFrom); !collision && c<=std::min((int)mask.size()-1,mask.cellTo); c++){
          collision |= (mask.d[c] & canvas[r*mask.size() + c]) != 0;
        }
      }
      return collision;
    }

    inline void markLabelPlace(std::vector<uint64_t> &canvas,
                               const Mask &mask,
                               int viewportHeight
    )
    {
      for (int r=std::max(0,mask.rowFrom); r<=std::min((int)viewportHeight-1, mask.rowTo); r++){
        for (int c=std::max(0,mask.cellFrom); c<=std::min((int)mask.size()-1, mask.cellTo); c++){
          canvas[r*mask.size() + c] = mask.d[c] | canvas[r*mask.size() + c];
        }
      }
    }

    void layout(int viewportWidth, int viewportHeight)
    {
      std::vector<ContourLabelType> allSortedContourLabels;
      std::vector<LabelInstanceType> allSortedLabels;

      std::swap(allSortedLabels, labelInstances);
      std::swap(allSortedContourLabels, contourLabelInstances);

      // TODO: sort labels by priority and position (to be deterministic)

      // compute collisions, hide some labels
      int64_t rowSize = (viewportWidth / 64)+1;
      //int64_t binaryWidth = rowSize * 8;
      //size_t binaryHeight = (viewportHeight / 8)+1;
      std::vector<uint64_t> canvas((size_t)(rowSize*viewportHeight));
      //canvas.data()

      auto labelIter = allSortedLabels.begin();
      auto contourLabelIter = allSortedContourLabels.begin();
      while (labelIter != allSortedLabels.end()
             || contourLabelIter != allSortedContourLabels.end()) {

        auto currentLabel = labelIter;
        auto currentContourLabel = contourLabelIter;
        if (currentLabel != allSortedLabels.end()
            && currentContourLabel != allSortedContourLabels.end()) {
          if (currentLabel->priority != currentContourLabel->priority) {
            if (currentLabel->priority < currentContourLabel->priority) {
              currentContourLabel = allSortedContourLabels.end();
            } else {
              currentLabel = allSortedLabels.end();
            }
          }
        }

        if (currentLabel != allSortedLabels.end()){

          IntRectangle rectangle{
              (int)currentLabel->x,
              (int)currentLabel->y,
              (int)currentLabel->label.width,
              (int)currentLabel->label.height,
          };
          Mask row(rowSize);

          row.prepare(rectangle);

          bool collision=checkLabelCollision(canvas, row, viewportHeight);
          if (!collision) {
            markLabelPlace(canvas, row, viewportHeight);
            labelInstances.push_back(*currentLabel);
          }

          labelIter++;
        }

        if (currentContourLabel != allSortedContourLabels.end()){
          int glyphCnt=currentContourLabel->glyphs.size();
          //std::vector<uint64_t> rowBuff((size_t)(rowSize * glyphCnt));
          Mask m(rowSize);
          std::vector<Mask> masks(glyphCnt, m);
          bool collision=false;
          for (int gi=0; !collision && gi<glyphCnt; gi++) {
            //uint64_t *row=rowBuff.data() + (gi*rowSize);

            auto glyph=currentContourLabel->glyphs[gi];
            IntRectangle rect{
                (int)glyph.trPosition.GetX(),
                (int)glyph.trPosition.GetY(),
                (int)glyph.trWidth,
                (int)glyph.trHeight
            };
            masks[gi].prepare(rect);
            collision |= checkLabelCollision(canvas, masks[gi], viewportHeight);
          }
          if (!collision) {
            for (int gi=0; gi<glyphCnt; gi++) {
              markLabelPlace(canvas, masks[gi], viewportHeight);
            }
            contourLabelInstances.push_back(*currentContourLabel);
          }
          contourLabelIter++;
        }
      }
    }

    void registerLabel(Vertex2D point,
                       std::string string,
                       double proposedWidth = 5000.0)
    {
      int fontHeight=18;
      LabelInstanceType instance;

      instance.label = textLayouter->layout(string, fontHeight, proposedWidth);

      instance.id = 0;
      instance.priority = 0;

      instance.x = point.GetX() - instance.label.width/2;
      instance.y = point.GetY() - instance.label.height/2;
      instance.alpha = 1.0;

      labelInstances.push_back(instance);
    }

    void registerContourLabel(std::vector<Vertex2D> way,
                              std::string string)
    {
      // TODO: parameters
      int fontHeight=24;
      int textOffset=fontHeight / 3;

      // TODO: cache simplified path for way id
      SimplifiedPath p;
      for (auto const &point:way){
        p.AddPoint(point.GetX(), point.GetY());
      }

      // TODO: cache label for string and font parameters
      LabelType label = textLayouter->layout(string, fontHeight, /* proposed width */ 5000.0);
      std::vector<Glyph<NativeGlyph>> glyphs = label.toGlyphs();

      double pLength=p.GetLength();
      double offset=0;
      while (offset<pLength){
        ContourLabelType cLabel;
        cLabel.priority = 1;
        for (Glyph<NativeGlyph> glyphCopy:glyphs){
          double glyphOffset=offset+glyphCopy.position.GetX();
          osmscout::Vertex2D point=p.PointAtLength(glyphOffset);
          double angle=p.AngleAtLength(glyphOffset)*-1;
          double  sinA=std::sin(angle);
          double  cosA=std::cos(angle);

          glyphCopy.position=osmscout::Vertex2D(point.GetX() - textOffset * sinA,
                                                point.GetY() + textOffset * cosA);

          glyphCopy.angle=angle;

          double w=glyphWidth(glyphCopy.glyph);
          double h=glyphHeight(glyphCopy.glyph);
          auto tl=glyphTopLeft(glyphCopy.glyph);

          // four coordinates of glyph bounding box; x,y of top-left, top-right, bottom-right, bottom-left
          std::array<double, 4> x{tl.GetX(), tl.GetX()+w, tl.GetX()+w, tl.GetX()};
          std::array<double, 4> y{tl.GetY(), tl.GetY(), tl.GetY()+h, tl.GetY()+h};

          // rotate
          for (int i=0; i<4; i++){
            double tmp;
            tmp  = x[i] * cosA - y[i] * sinA;
            y[i] = x[i] * sinA + y[i] * cosA;
            x[i] = tmp;
          }
          glyphCopy.tl.Set(x[0]+glyphCopy.position.GetX(), y[0]+glyphCopy.position.GetY());
          glyphCopy.tr.Set(x[1]+glyphCopy.position.GetX(), y[1]+glyphCopy.position.GetY());
          glyphCopy.br.Set(x[2]+glyphCopy.position.GetX(), y[2]+glyphCopy.position.GetY());
          glyphCopy.bl.Set(x[3]+glyphCopy.position.GetX(), y[3]+glyphCopy.position.GetY());

          // bounding box
          double minX=x[0];
          double maxX=x[0];
          double minY=y[0];
          double maxY=y[0];
          for (int i=1; i<4; i++){
            minX = std::min(minX, x[i]);
            maxX = std::max(maxX, x[i]);
            minY = std::min(minY, y[i]);
            maxY = std::max(maxY, y[i]);
          }
          // setup glyph top-left position and dimension after rotation
          glyphCopy.trPosition.Set(minX+glyphCopy.position.GetX(), minY+glyphCopy.position.GetY());
          glyphCopy.trWidth  = maxX - minX;
          glyphCopy.trHeight = maxY - minY;

          cLabel.glyphs.push_back(glyphCopy);
        }
        contourLabelInstances.push_back(cLabel);
        offset+=label.width + 3*fontHeight;
      }
    }

    std::vector<LabelInstanceType> labels() const
    {
      return labelInstances;
    }

    std::vector<ContourLabelType> contourLabels() const
    {
      return contourLabelInstances;
    }

  private:
    TextLayouter *textLayouter;
    std::vector<ContourLabelType> contourLabelInstances;
    std::vector<LabelInstanceType> labelInstances;
  };

}

#endif
