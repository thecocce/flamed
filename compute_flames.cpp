// Copyright (C) 2011, Chris Foster and the other authors and contributors.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// * Neither the name of the software's owners nor the names of its
//   contributors may be used to endorse or promote products derived from this
//   software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
// (This is the New BSD license)

#include "compute_flames.h"


// TODO: Fix these to deal properly with the finalMap and factor common code

void FlameMapping::translate(V2f p, V2f df, bool editPreTrans)
{
    FlameMapping tmpMap = *this;
    AffineMap& aff = editPreTrans ? tmpMap.preMap : tmpMap.postMap;
    const float delta = 0.001;
    V2f r0 = tmpMap.map(p);
    aff.c.x += delta;
    V2f r1 = tmpMap.map(p);
    aff.c.x -= delta;
    aff.c.y += delta;
    V2f r2 = tmpMap.map(p);
    V2f drdx = (r1 - r0)/delta;
    V2f drdy = (r2 - r0)/delta;
    M22f dfdc(drdx.x, drdy.x,
                drdx.y, drdy.y);
    V2f dc = dfdc.inv()*df;
    const float maxLength = 2;
    if(dc.length() > maxLength)
        dc *= maxLength/dc.length();
    AffineMap& thisAff = editPreTrans ? preMap : postMap;
    thisAff.c += dc;
}

void FlameMapping::scale(V2f p, V2f df, bool editPreTrans)
{
    FlameMapping tmpMap = *this;
    AffineMap& aff = editPreTrans ? tmpMap.preMap : tmpMap.postMap;
    const float delta = 0.001;
    V2f r0 = tmpMap.map(p);
    aff.m.a *= (1+delta);
    aff.m.c *= (1+delta);
    V2f r1 = tmpMap.map(p);
    tmpMap = *this;
    aff.m.b *= (1+delta);
    aff.m.d *= (1+delta);
    V2f r2 = tmpMap.map(p);
    V2f drdx = (r1 - r0)/delta;
    V2f drdy = (r2 - r0)/delta;
    M22f dfdad(drdx.x, drdy.x,
               drdx.y, drdy.y);
    V2f d_ad = dfdad.inv()*df;
    AffineMap& thisAff = editPreTrans ? preMap : postMap;
    thisAff.m.a *= (1+d_ad.x);
    thisAff.m.c *= (1+d_ad.x);
    thisAff.m.b *= (1+d_ad.y);
    thisAff.m.d *= (1+d_ad.y);
}

void FlameMapping::rotate(V2f p, V2f df, bool editPreTrans)
{
    FlameMapping tmpMap = *this;
    AffineMap& aff = editPreTrans ? tmpMap.preMap : tmpMap.postMap;
    const float delta = 0.001;
    V2f r0 = tmpMap.map(p);
    aff.m = M22f( cos(delta), sin(delta),
            -sin(delta), cos(delta)) * aff.m;
    V2f r1 = tmpMap.map(p);
    V2f dfdtheta = (r1 - r0)/delta;
    tmpMap = *this;
    aff.m *= (1 + delta);
    V2f r2 = tmpMap.map(p);
    V2f dfdscale = (r2 - r0)/delta;
    M22f dfdts(dfdtheta.x, dfdscale.x,
               dfdtheta.y, dfdscale.y);
    V2f dts = dfdts.inv()*df;
    float dtheta = dts.x;
    float dscale = dts.y;
    // Least squares for single parameter
    //float dtheta = dot(df, dfdtheta) / dot(dfdtheta, dfdtheta);
    AffineMap& thisAff = editPreTrans ? preMap : postMap;
    thisAff.m = (1 + dscale) * M22f( cos(dtheta), sin(dtheta),
                                    -sin(dtheta), cos(dtheta)) * thisAff.m;
}


std::ostream& operator<<(std::ostream& out, const FlameMapping& map)
{
    out << map.preMap << "\n"
        << map.variation << "\n"
        << map.postMap << "\n"
        << map.col << "   " << map.colorSpeed;
    return out;
}


void FlameMaps::save(std::ostream& out)
{
    out << "FlamEd V1\n";
    out.precision(8);
    out << hdrExposure << " " << hdrPow << "\n";
    out << finalMap << "\n";
    for(size_t i = 0; i < maps.size(); ++i)
        out << "--\n" << maps[i] << "\n";
    out << "----\n";
}


static bool loadMap(std::istream& in, FlameMapping& map)
{
    in >> map.preMap
       >> map.variation
       >> map.postMap
       >> map.col >> map.colorSpeed;
    return true; // TODO: error checking.
}


bool FlameMaps::load(std::istream& in)
{
    FlameMaps tmpMaps;
    std::string s;
    in >> s;
    if(s != "FlamEd")
        return false;
    in >> s;
    if(s != "V1")
        return false;
    in >> tmpMaps.hdrExposure >> tmpMaps.hdrPow;
    loadMap(in, tmpMaps.finalMap);
    in >> s;
    while(s == "--")
    {
        tmpMaps.maps.push_back(FlameMapping());
        loadMap(in, tmpMaps.maps.back());
        in >> s;
    }
    if(s != "----")
        return false;
    *this = tmpMaps;
    return true;
}


//------------------------------------------------------------------------------
void CPUFlameEngine::generate(PointVBO* points, const FlameMaps& flameMaps)
{
    IFSPoint* ptData = points->mapBuffer(GL_WRITE_ONLY);

    int nMaps = flameMaps.maps.size();
    V2f p = V2f(0,0);
    int discard = 20;
    C3f col(1);
    int nPoints = points->size();
    for(int i = -discard; i < nPoints; ++i)
    {
        int mapIdx = rand() % nMaps;
        const FlameMapping& m = flameMaps.maps[mapIdx];
        p = m.map(p);
        col = m.colorSpeed*m.col + (1-m.colorSpeed)*col;
        if(i >= 0)
        {
            ptData->pos = flameMaps.finalMap.map(p);
            ptData->col = col;
            ++ptData;
        }
    }
    points->unmapBuffer();
}

