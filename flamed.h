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

#ifndef FLAMED_H_INCLUDED
#define FLAMED_H_INCLUDED

#include <QtOpenGL/QGLWidget>
#include <QtGui/QMainWindow>

#include "util.h"

class QGLShaderProgram;
class QGLFramebufferObject;
class QTimer;
class QImage;
class FlameEngine;
template<typename> class VertexBufferObject;
struct IFSPoint;
typedef VertexBufferObject<IFSPoint> PointVBO;
namespace Poppler { class Document; }

struct FlameMaps;

#include <tr1/memory>
// Note: gcc's tr1::shared_ptr doesn't work with nvcc, which is why this isn't
// used in util.h :(
using std::tr1::shared_ptr;


/// Viewer for flame fractals
class FlameViewWidget : public QGLWidget
{
    Q_OBJECT
    public:
        FlameViewWidget(QWidget* parent = 0);

        /// Load fractal from file
        void load(const char* fileName);
        /// Save current fractal to file
        void save(const char* fileName);

    protected:
        // GL stuff
        void initializeGL();
        void resizeGL(int w, int h);
        void paintGL();

        // User events
        void keyPressEvent(QKeyEvent* event);
        void mousePressEvent(QMouseEvent* event);
        void mouseMoveEvent(QMouseEvent* event);
        void mouseReleaseEvent(QMouseEvent* event);

        QSize sizeHint() const;

    private:
        enum EditMode
        {
            Mode_Translate,
            Mode_Rotate,
            Mode_Scale
        };

        static shared_ptr<FlameMaps> initMaps();
        void loadScreenCoords() const;
        void drawMaps(const FlameMaps* flameMaps);
        void clearAccumulator();

        shared_ptr<QGLShaderProgram> m_pointRenderProgram;
        shared_ptr<QGLShaderProgram> m_hdriProgram;
        shared_ptr<QGLFramebufferObject> m_pointAccumFBO;
        shared_ptr<PointVBO> m_ifsPoints;
        shared_ptr<FlameMaps> m_flameMaps;
        shared_ptr<FlameEngine> m_flameEngine;
        std::vector<shared_ptr<FlameMaps> > m_undoList;
        std::vector<shared_ptr<FlameMaps> > m_redoList;
        bool m_useGpu;
        bool m_whiteBackground;

        bool m_editMaps;
        EditMode m_editMode;
        int m_mapToEdit;
        bool m_editPreTransform;
        QPoint m_lastPos;
        V2f m_invPick;

        QTimer* m_frameTimer;
        int m_nPasses;
};


//------------------------------------------------------------------------------
/// Main window for fractal flame viewer housing menu logic, etc.
class FlamedMainWindow : public QMainWindow
{
    Q_OBJECT
    public:
        FlamedMainWindow();

        FlameViewWidget& flameView() { return *m_flameView; }

    private slots:
        void openFile();
        void saveFile();
        void exportBitmap();
        void toggleFullscreen();

    private:
        FlameViewWidget* m_flameView;
};

#endif // FLAMED_H_INCLUDED
