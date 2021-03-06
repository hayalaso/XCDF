
/*
Copyright (c) 2014, University of Maryland
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

#ifndef XCDF_UTILITY_EVENT_SELECT_EXPRESSION_INCLUDED_H
#define XCDF_UTILITY_EVENT_SELECT_EXPRESSION_INCLUDED_H

#include <xcdf/utility/Symbol.h>
#include <xcdf/utility/NodeDefs.h>
#include <xcdf/utility/Expression.h>
#include <xcdf/XCDFPtr.h>

#include <vector>
#include <list>
#include <cassert>
#include <sstream>

// Forward-declare XCDFFile to avoid circular dependency introduced
// with XCDFFieldAlias.  There should be a cleaner way to code this.
class XCDFFile;

class EventSelectExpression {

  public:

    EventSelectExpression(const std::string& exp,
                          const XCDFFile& f) :
                expression_(xcdf_shared(new Expression(exp, f))) {

      Symbol* start = expression_->GetHeadSymbol();
      switch (start->GetType()) {

        case FLOATING_POINT_NODE:
          selectNode_ = XCDFPtr<Node<uint64_t> >(
             new AnyNode<double>(*static_cast<Node<double>* >(start)));
          break;

        case SIGNED_NODE:
          selectNode_ = XCDFPtr<Node<uint64_t> >(
             new AnyNode<int64_t>(*static_cast<Node<int64_t>* >(start)));
          break;

        case UNSIGNED_NODE:
          selectNode_ = XCDFPtr<Node<uint64_t> >(
             new AnyNode<uint64_t>(*static_cast<Node<uint64_t>* >(start)));
          break;

        default:
          XCDFFatal("Expression does not evaluate: " << exp);
      }
    }

    // We know AnyNode is always size 1
    bool SelectEvent() const {return (*selectNode_)[0];}

  private:

    XCDFPtr<Expression> expression_;
    XCDFPtr<Node<uint64_t> > selectNode_;
};

#endif // XCDF_UTILITY_EVENT_SELECT_EXPRESSION_INCLUDED_H
