
/*
Copyright (c) 2014, University of Maryland
                    Jim Braun
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef XCDF_UTILITY_HISTOGRAM_H_INCLUDED
#define XCDF_UTILITY_HISTOGRAM_H_INCLUDED

#include <xcdf/XCDFDefs.h>
#include <xcdf/utility/NumericalExpression.h>

#include <vector>
#include <utility>
#include <stdint.h>
#include <cmath>
#include <limits>
#include <iostream>

class Histogram1D {

  public:

    Histogram1D(unsigned nbins, double min, double max) : data_(nbins, 0.),
                                                          dataW2_(nbins, 0.),
                                                          underflow_(0.),
                                                          underflowW2_(0.),
                                                          overflow_(0.),
                                                          overflowW2_(0.),
                                                          min_(min),
                                                          max_(max) {

      if (nbins == 0) {
        XCDFFatal("Histogram must have >0 bins");
      }

      if (!(max > min)) {
        XCDFFatal("Histogram maximum must be larger than the minimum");
      }
      rinv_ = 1. / (max - min);
    }

    unsigned GetNBins() const {return data_.size();}
    double GetMinimum() const {return min_;}
    double GetMaximum() const {return max_;}
    double GetBinMinimum(unsigned i) const {
      return min_ + (i+0.) / (rinv_ * GetNBins());
    }
    double GetBinCenter(unsigned i) const {
      return min_ + (i+0.5) / (rinv_ * GetNBins());
    }
    double GetUnderflow() const {return underflow_;}
    double GetOverflow() const {return overflow_;}
    double GetUnderflowW2Sum() const {return underflowW2_;}
    double GetOverflowW2Sum() const {return overflowW2_;}
    double GetData(unsigned i) const {return data_[i];}
    double GetW2Sum(unsigned i) const {return dataW2_[i];}
    double operator[](unsigned i) const {return GetData(i);}

    void Fill(double value, double weight=1.) {

      double ldiff = (value - min_) * rinv_ * GetNBins();
      // Don't let integers at bin edges round down!
      ldiff *= (1. + std::numeric_limits<double>::epsilon());
      if (ldiff < 0.) {
        underflow_ += weight;
        underflowW2_ += weight*weight;
      } else if (ldiff >= GetNBins()) {
        overflow_ += weight;
        overflowW2_ += weight*weight;
      } else {
        int64_t binno = static_cast<int64_t>(floor(ldiff));
        data_[binno] += weight;
        dataW2_[binno] += weight*weight;
      }
    }

    friend class Histogram2D;

  private:

    std::vector<double> data_;
    std::vector<double> dataW2_;
    double underflow_;
    double underflowW2_;
    double overflow_;
    double overflowW2_;

    double min_;
    double max_;
    double rinv_;
};

class Histogram2D {

  public:

    Histogram2D(unsigned nbinsX, double minX, double maxX,
                unsigned nbinsY, double minY, double maxY) :
                                              data_(nbinsX*nbinsY, 0.),
                                              dataW2_(nbinsX*nbinsY, 0.),
                                              nbinsX_(nbinsX),
                                              nbinsY_(nbinsY),
                                              xMin_(minX),
                                              xMax_(maxX),
                                              yMin_(minY),
                                              yMax_(maxY) {

      if (nbinsX == 0 || nbinsY == 0) {
        XCDFFatal("Histogram must have >0 bins");
      }

      if (!(maxX > minX) || !(maxY > minY)) {
        XCDFFatal("Histogram maximum must be larger than the minimum");
      }
      xRinv_ = 1. / (maxX - minX);
      yRinv_ = 1. / (maxY - minY);
    }

    unsigned GetNBins() const {return data_.size();}
    unsigned GetNBinsX() const {return nbinsX_;}
    unsigned GetNBinsY() const {return nbinsY_;}
    double GetXMinimum() const {return xMin_;}
    double GetXMaximum() const {return xMax_;}
    double GetYMinimum() const {return yMin_;}
    double GetYMaximum() const {return yMax_;}
    std::pair<double, double> GetBinMinimum(unsigned i) const {
      return GetBinMinimum(i % nbinsX_, i / nbinsX_);
    }
    std::pair<double, double> GetBinMinimum(unsigned i, unsigned j) const {
      double mx = xMin_ + (i+0.) / (xRinv_ * GetNBinsX());
      double my = yMin_ + (j+0.) / (yRinv_ * GetNBinsY());
      return std::pair<double, double>(mx, my);
    }
    std::pair<double, double> GetBinCenter(unsigned i) const {
      return GetBinCenter(i % nbinsX_, i / nbinsX_);
    }
    std::pair<double, double> GetBinCenter(unsigned i, unsigned j) const {
      double mx = xMin_ + (i+0.5) / (xRinv_ * GetNBinsX());
      double my = yMin_ + (j+0.5) / (yRinv_ * GetNBinsY());
      return std::pair<double, double>(mx, my);
    }
    double GetData(unsigned i) const {return data_[i];}
    double GetW2Sum(unsigned i) const {return dataW2_[i];}
    double operator[](unsigned i) const {return GetData(i);}
    double GetData(unsigned i, unsigned j) const {
      return data_[j*nbinsX_ + i];
    }
    double GetW2Sum(unsigned i, unsigned j) const {
      return dataW2_[j*nbinsX_ + i];
    }

    void Fill(double xValue, double yValue, double weight=1.) {

      double xdiff = (xValue - xMin_) * xRinv_ * nbinsX_;
      double ydiff = (yValue - yMin_) * yRinv_ * nbinsY_;
      // Don't let integers at bin edges round down!
      xdiff *= (1. + std::numeric_limits<double>::epsilon());
      ydiff *= (1. + std::numeric_limits<double>::epsilon());
      if (xdiff >= 0. && xdiff < nbinsX_ &&
          ydiff >= 0. && ydiff < nbinsY_) {

        int64_t binnoX = static_cast<int64_t>(xdiff);
        int64_t binnoY = static_cast<int64_t>(ydiff);
        int64_t bb = binnoY * nbinsX_ + binnoX;
        data_[bb] += weight;
        dataW2_[bb] += weight*weight;
      }
    }

    Histogram1D ProfileX(unsigned i) {
      return ProfileX(std::vector<unsigned>(1, i));
    }

    Histogram1D ProfileX(const std::vector<unsigned> yBins) {
      Histogram1D out(nbinsX_, xMin_, xMax_);
      for (unsigned i = 0; i < yBins.size(); ++i) {
        for (unsigned j = 0; j < nbinsX_; ++j) {
          unsigned ibn = yBins[i]*nbinsX_ + j;
          out.data_[j] += data_[ibn];
          out.dataW2_[j] += dataW2_[ibn];
        }
      }
      return out;
    }

    Histogram1D ProfileY(unsigned i) {
      return ProfileY(std::vector<unsigned>(1, i));
    }

    Histogram1D ProfileY(const std::vector<unsigned> xBins) {
      Histogram1D out(nbinsY_, yMin_, yMax_);
      for (unsigned i = 0; i < xBins.size(); ++i) {
        for (unsigned j = 0; j < nbinsY_; ++j) {
          unsigned ibn = j*nbinsX_ + xBins[i];
          out.data_[j] += data_[ibn];
          out.dataW2_[j] += dataW2_[ibn];
        }
      }
      return out;
    }

  private:

    std::vector<double> data_;
    std::vector<double> dataW2_;
    unsigned nbinsX_;
    unsigned nbinsY_;

    double xMin_;
    double xMax_;
    double yMin_;
    double yMax_;
    double xRinv_;
    double yRinv_;
};

class Filler1D {

  public:

    Filler1D(const std::string& xExpr,
             const std::string& wExpr) : xExpr_(xExpr), wExpr_(wExpr) { }

    void Fill(Histogram1D& h, XCDFFile& f) {

      NumericalExpression<double> xne(xExpr_, f);
      NumericalExpression<double> wne(wExpr_, f);

      while (f.Read()) {
        h.Fill(xne.Evaluate(), wne.Evaluate());
      }
    }

  private:

    std::string xExpr_;
    std::string wExpr_;
};

class Filler2D {

  public:

    Filler2D(const std::string& xExpr,
             const std::string& yExpr,
             const std::string& wExpr) : xExpr_(xExpr),
                                         yExpr_(yExpr),
                                         wExpr_(wExpr) { }

    void Fill(Histogram2D& h, XCDFFile& f) {

      NumericalExpression<double> xne(xExpr_, f);
      NumericalExpression<double> yne(yExpr_, f);
      NumericalExpression<double> wne(wExpr_, f);

      while (f.Read()) {
        h.Fill(xne.Evaluate(), yne.Evaluate(), wne.Evaluate());
      }
    }

  private:

    std::string xExpr_;
    std::string yExpr_;
    std::string wExpr_;
};

std::ostream& operator<<(std::ostream& out, const Histogram1D& h) {

  out << std::setw(11) << "X" << " Value" << std::endl;
  for (unsigned i = 0; i < h.GetNBins(); ++i) {
    out << std::setw(11) << h.GetBinCenter(i);
    out << " " << h.GetData(i) << std::endl;
  }
  out << std::endl;
  return out;
}

std::ostream& operator<<(std::ostream& out, const Histogram2D& h) {

  out << std::setw(8) << "X" << " ";
  out << std::setw(8) << "Y" << " Value" << std::endl;
  for (unsigned i = 0; i < h.GetNBins(); ++i) {
    out << std::setw(8) << h.GetBinCenter(i).first << " ";
    out << std::setw(8) << h.GetBinCenter(i).second;
    out << " " << h.GetData(i) << std::endl;
  }
  out << std::endl;
  return out;
}

#endif // XCDF_UTILITY_HISTOGRAM_H_INCLUDED
