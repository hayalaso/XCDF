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

#ifndef XCDF_UTILITY_FIELD_NODE_DEFS_H_INCLUDED
#define XCDF_UTILITY_FIELD_NODE_DEFS_H_INCLUDED

/*
 * Nodes dependent on XCDFFile, XCDFField, and XCDFAlias
 * need a separate header, visible only when compiling
 * Expression.  Otherwise we introduce circular dependencies
 * with XCDFAlias.
 */

#include <xcdf/XCDFFile.h>
#include <xcdf/alias/XCDFFieldAlias.h>
#include <xcdf/XCDFField.h>

template <typename T>
class FieldNode : public Node<T> {

  public:

    FieldNode(ConstXCDFField<T> field) : field_(field) { }

    T operator[](unsigned index) const {return field_[index];}
    unsigned GetSize() const {return field_.GetSize();}

    bool HasParent() const {return field_.HasParent();}
    const std::string& GetParentName() const {return field_.GetParentName();}
    const std::string& GetName() const {return field_.GetName();}

  private:

    ConstXCDFField<T> field_;
};

template <typename T>
class AliasNode : public Node<T> {

  public:

    AliasNode(const XCDFFieldAlias<T>& alias) : alias_(alias) { }

    T operator[](unsigned index) const {return alias_[index];}
    unsigned GetSize() const {return alias_.GetSize();}

    bool HasParent() const {return alias_.GetHeadNode().HasParent();}
    const std::string& GetParentName() const {
      return alias_.GetHeadNode().GetParentName();
    }
    const std::string& GetName() const {return alias_.GetName();}

  private:

    XCDFFieldAlias<T> alias_;
};

class CounterNode : public Node<uint64_t> {

  public:

    CounterNode(const XCDFFile& f) : f_(f) { }

    uint64_t operator[](unsigned index) const {
      return f_.GetCurrentEventNumber();
    }
    unsigned GetSize() const {return 1;}

  private:

    const XCDFFile& f_;
};

#endif // XCDF_UTILITY_FIELD_NODE_DEFS_H_INCLUDED
