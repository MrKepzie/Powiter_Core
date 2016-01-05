/* ***** BEGIN LICENSE BLOCK *****
 * This file is part of Natron <http://www.natron.fr/>,
 * Copyright (C) 2016 INRIA and Alexandre Gauthier-Foichat
 *
 * Natron is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * Natron is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Natron.  If not, see <http://www.gnu.org/licenses/gpl-2.0.html>
 * ***** END LICENSE BLOCK ***** */

#ifndef APPINSTANCE_H
#define APPINSTANCE_H

// ***** BEGIN PYTHON BLOCK *****
// from <https://docs.python.org/3/c-api/intro.html#include-files>:
// "Since Python may define some pre-processor definitions which affect the standard headers on some systems, you must include Python.h before any standard headers are included."
#include <Python.h>
// ***** END PYTHON BLOCK *****

#include <vector>
#include <list>

#if !defined(Q_MOC_RUN) && !defined(SBK_RUN)
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#endif
#include <QStringList>

#include "Global/GlobalDefines.h"
#include "Engine/RectD.h"
#include "Engine/TimeLineKeyFrames.h"
#include "Engine/EngineFwd.h"

struct AppInstancePrivate;

struct CreateNodeArgs
{
    typedef std::list< boost::shared_ptr<KnobSerialization> > DefaultValuesList;

    ///The pluginID corresponds to something generated by
    ///OfxEffectInstance::generateImageEffectClassName
    ///or given from the virtual function getPluginID() of the builtin plugins (such as Backdrop,Viewer,etc...)
    QString pluginID;
    int majorV,minorV;
    std::string multiInstanceParentName;
    double xPosHint, yPosHint;
    QString fixedName;

    DefaultValuesList paramValues; //< values of parameters to set before creating the plug-in
    boost::shared_ptr<NodeCollection> group;

    bool autoConnect;
    bool pushUndoRedoCommand;
    bool userEdited;
    bool addToProject;
    bool createGui;

    ///Constructor used to create a new node
    explicit CreateNodeArgs(const QString & pluginID, //< the pluginID (as they appear in the "Tab" menu in the nodegraph)
                            const std::string & multiInstanceParentName, //< no parent by default DEFAULT = empty
                            int majorVersion , //< use greatest version found DEFAULT = - 1
                            int minorVersion , //< use greatest version found DEFAULT = -1
                            bool autoConnect , //< should we attempt to autoconnect ? DEFAULT = true
                            double xPosHint, //< xPosition in the nodegraph DEFAULT = INT_MIN
                            double yPosHint, //< yPosition in the nodegraph DEFAULT = INT_MIN
                            bool pushUndoRedoCommand , //< should we push a new undo/redo command on the GUI? DEFAULT = true
                            bool addToProject, //< should we add the node to the project ? DEFAULT = true
                            bool userEdited, //< was it called from userAction ? (used to show file dialogs)
                            const QString & fixedName,  //< if non empty, this will be the name of the node DEFAULT = empty
                            const DefaultValuesList& paramValues,  //< parameters to set before creating the plugin
                            const boost::shared_ptr<NodeCollection>& group) //< the group into which to create this node
        : pluginID(pluginID)
    , majorV(majorVersion)
    , minorV(minorVersion)
    , multiInstanceParentName(multiInstanceParentName)
    , xPosHint(xPosHint)
    , yPosHint(yPosHint)
    , fixedName(fixedName)
    , paramValues(paramValues)
    , group(group)
    , autoConnect(autoConnect)
    , pushUndoRedoCommand(pushUndoRedoCommand)
    , userEdited(userEdited)
    , addToProject(addToProject)
    , createGui(true)
    {
    }
    
};


struct LoadNodeArgs
{
    QString pluginID;
    std::string multiInstanceParentName;
    const NodeSerialization* serialization;
    boost::shared_ptr<NodeCollection> group;
    int majorV;
    int minorV;
    bool dontLoadName;

    ///Constructor used to load a node from the project serialization
    LoadNodeArgs(const QString & pluginID,
                 const std::string & multiInstanceParentName,
                 int majorVersion,
                 int minorVersion,
                 const NodeSerialization* serialization,
                 bool dontLoadName,
                 const boost::shared_ptr<NodeCollection>& group)
        : pluginID(pluginID)
    , multiInstanceParentName(multiInstanceParentName)
    , serialization(serialization)
    , group(group)
    , majorV(majorVersion)
    , minorV(minorVersion)
    , dontLoadName(dontLoadName) //< used when copy/pasting nodes to avoid duplicates in name
    {
    }
};

namespace Natron {
    
class FlagSetter {
    
    bool* p;
    QMutex* lock;
    
public:
    
    FlagSetter(bool initialValue,bool* p);
    
    FlagSetter(bool initialValue,bool* p, QMutex* mutex);
    
    ~FlagSetter();
};
    
}

class AppInstance
    : public QObject, public boost::noncopyable, public TimeLineKeyFrames
{
    Q_OBJECT

public:


    AppInstance(int appID);

    virtual ~AppInstance();

    virtual void aboutToQuit();
    
    virtual bool isBackground() const { return true; }
    
    struct RenderRequest {
        QString writerName;
        int firstFrame,lastFrame,frameStep;
    };
    
    struct RenderWork {
        Natron::OutputEffectInstance* writer;
        int firstFrame;
        int lastFrame;
        int frameStep;
    };
    
    virtual void load(const CLArgs& cl,bool makeEmptyInstance);

    int getAppID() const;

    /** @brief Create a new node  in the node graph.
     * The name passed in parameter must match a valid node name,
     * otherwise an exception is thrown. You should encapsulate the call
     * by a try-catch block.
     * If the majorVersion is not -1 then this function will attempt to find a plugin with the matching
     * majorVersion, or otherwise it will throw an exception.
     * If the minorVersion is not -1 then this function will attempt to load a plugin with the greatest minorVersion
     * greater or equal to this minorVersion.
     * By default this function also create the node's graphical user interface and attempts to automatically
     * connect this node to other nodes selected.
     * If requestedByLoad is true then it will never attempt to do this auto-connection.
     * If openImageFileDialog is true then if the node has a file knob indicating an image file it will automatically
     * prompt the user with a file dialog.
     *
     * @param createGui If false, the node GUI (in the nodegraph and in the properties panel) will not be created.
     * The node animation curves will still be available in the curve editor though.
     *
     * You can use this function to create backdrops also which are purely GUI stuff. In this case the pointer returned will
     * be NULL.
     **/
    boost::shared_ptr<Natron::Node> createNode(const CreateNodeArgs & args);

    ///Same as createNode but used when loading a project
    boost::shared_ptr<Natron::Node> loadNode(const LoadNodeArgs & args);
  
    boost::shared_ptr<Natron::Node> getNodeByFullySpecifiedName(const std::string & name) const;
    
    boost::shared_ptr<Natron::Project> getProject() const;
    
    boost::shared_ptr<TimeLine> getTimeLine() const;

    /*true if the user is NOT scrubbing the timeline*/
    virtual bool shouldRefreshPreview() const
    {
        return false;
    }

    virtual void connectViewersToViewerCache()
    {
    }

    virtual void disconnectViewersFromViewerCache()
    {
    }

    virtual void errorDialog(const std::string & title,const std::string & message,bool useHtml) const;
    virtual void errorDialog(const std::string & title,const std::string & message,bool* stopAsking,bool useHtml) const;
    virtual void warningDialog(const std::string & title,const std::string & message,bool useHtml) const;
    virtual void warningDialog(const std::string & title,const std::string & message,bool* stopAsking,bool useHtml) const;
    virtual void informationDialog(const std::string & title,const std::string & message,bool useHtml) const;
    virtual void informationDialog(const std::string & title,const std::string & message,bool* stopAsking,bool useHtml) const;
    
    virtual Natron::StandardButtonEnum questionDialog(const std::string & title,
                                                      const std::string & message,
                                                      bool useHtml,
                                                      Natron::StandardButtons buttons =
                                                      Natron::StandardButtons(Natron::eStandardButtonYes | Natron::eStandardButtonNo),
                                                  Natron::StandardButtonEnum defaultButton = Natron::eStandardButtonNoButton) const WARN_UNUSED_RETURN;
    
    /**
     * @brief Asks a question to the user and returns the reply.
     * @param stopAsking Set to true if the user do not want Natron to ask the question again.
     **/
    virtual Natron::StandardButtonEnum questionDialog(const std::string & /*title*/,
                                                      const std::string & /*message*/,
                                                      bool /*useHtml*/,
                                                      Natron::StandardButtons /*buttons*/,
                                                      Natron::StandardButtonEnum /*defaultButton*/,
                                                      bool* /*stopAsking*/)
    {
        return Natron::eStandardButtonYes;
    }

    
    virtual void loadProjectGui(boost::archive::xml_iarchive & /*archive*/) const
    {
    }

    virtual void saveProjectGui(boost::archive::xml_oarchive & /*archive*/)
    {
    }

    virtual void setupViewersForViews(const std::vector<std::string>& /*viewNames*/)
    {
    }

    virtual void notifyRenderProcessHandlerStarted(const QString & /*sequenceName*/,
                                                   int /*firstFrame*/,
                                                   int /*lastFrame*/,
                                                   int /*frameStep*/,
                                                   const boost::shared_ptr<ProcessHandler> & /*process*/)
    {
    }

    virtual bool isShowingDialog() const
    {
        return false;
    }
    
    virtual bool isGuiFrozen() const { return false; }

    virtual void progressStart(KnobHolder* effect,
                               const std::string &message,
                               const std::string &messageid,
                               bool canCancel = true)
    {
        Q_UNUSED(effect);
        Q_UNUSED(message);
        Q_UNUSED(messageid);
        Q_UNUSED(canCancel);
    }

    virtual void progressEnd(KnobHolder* /*effect*/)
    {
    }

    virtual bool progressUpdate(KnobHolder* /*effect*/,
                                double /*t*/)
    {
        return true;
    }

    /**
     * @brief Checks for a new version of Natron
     **/
    void checkForNewVersion() const;
    virtual void onMaxPanelsOpenedChanged(int /*maxPanels*/)
    {
    }

    Natron::ViewerColorSpaceEnum getDefaultColorSpaceForBitDepth(Natron::ImageBitDepthEnum bitdepth) const;
    
    double getProjectFrameRate() const;

    virtual std::string openImageFileDialog() { return std::string(); }
    virtual std::string saveImageFileDialog() { return std::string(); }

    
    void onOCIOConfigPathChanged(const std::string& path);
    
  
    
    void startWritersRendering(bool enableRenderStats, bool doBlockingRender, const std::list<RenderRequest>& writers);
    void startWritersRendering(bool enableRenderStats, bool doBlockingRender, const std::list<RenderWork>& writers);

    void startRenderingBlockingFullSequence(bool enableRenderStats,const RenderWork& writerWork,bool renderInSeparateProcess,const QString& savePath);
    
    virtual void startRenderingFullSequence(bool enableRenderStats,const RenderWork& writerWork,bool renderInSeparateProcess,const QString& savePath);

    virtual void clearViewersLastRenderedTexture() {}

    virtual void toggleAutoHideGraphInputs() {}

    /**
     * @brief In Natron v1.0.0 plug-in IDs were lower case only due to a bug in OpenFX host support.
     * To be able to load projects created in Natron v1.0.0 we must identity that the project was created in this version
     * and try to load plug-ins with their lower case ID instead.
     **/
    void setProjectWasCreatedWithLowerCaseIDs(bool b);
    bool wasProjectCreatedWithLowerCaseIDs() const;
    
    bool isCreatingPythonGroup() const;
    
    bool isCreatingNodeTree() const;
    
    void setIsCreatingNodeTree(bool b);
    
    virtual void appendToScriptEditor(const std::string& str);
    
    virtual void printAutoDeclaredVariable(const std::string& str);
    
    void getFrameRange(double* first,double* last) const;
    
    virtual void setLastViewerUsingTimeline(const boost::shared_ptr<Natron::Node>& /*node*/) {}
    
    virtual ViewerInstance* getLastViewerUsingTimeline() const { return 0; }
    
    bool loadPythonScript(const QFileInfo& file);
    
    boost::shared_ptr<Natron::Node> createWriter(const std::string& filename,
                                                 const boost::shared_ptr<NodeCollection>& collection,
                                                 bool userEdited,
                                                 int firstFrame = INT_MIN, int lastFrame = INT_MAX);
    virtual void queueRedrawForAllViewers() {}
    
    virtual void renderAllViewers(bool /* canAbort*/) {}
    
    virtual void declareCurrentAppVariable_Python();

    void execOnProjectCreatedCallback();
    
    virtual void createLoadProjectSplashScreen(const QString& /*projectFile*/) {}
    
    virtual void updateProjectLoadStatus(const QString& /*str*/) {}
    
    virtual void closeLoadPRojectSplashScreen() {}
    
    std::string getAppIDString() const;
    
    void setCreatingNode(bool b);
    bool isCreatingNode() const;
    
    virtual bool isDraftRenderEnabled() const { return false; }
    
    virtual void setUserIsPainting(const boost::shared_ptr<Natron::Node>& /*rotopaintNode*/,
                                   const boost::shared_ptr<RotoStrokeItem>& /*stroke*/,
                                  bool /*isPainting*/) {}
    
    virtual void getActiveRotoDrawingStroke(boost::shared_ptr<Natron::Node>* /*node*/,
                                            boost::shared_ptr<RotoStrokeItem>* /*stroke*/,
                                            bool* /*isPainting*/) const { }
    
    virtual bool isRenderStatsActionChecked() const { return false; }
    
    bool saveTemp(const std::string& filename);
    virtual bool save(const std::string& filename);
    virtual bool saveAs(const std::string& filename);
    virtual AppInstance* loadProject(const std::string& filename);
    
    ///Close the current project but keep the window
    virtual bool resetProject();
    
    ///Reset + close window, quit if last window
    virtual bool closeProject();
    
    ///Opens a new window
    virtual AppInstance* newProject();
    
    virtual void* getOfxHostOSHandle() const { return NULL; }
    
    virtual void updateLastPaintStrokeData(int /*newAge*/,
                                           const std::list<std::pair<Natron::Point,double> >& /*points*/,
                                           const RectD& /*lastPointsBbox*/,
                                           int /*strokeIndex*/) {}
    
    virtual void getLastPaintStrokePoints(std::list<std::list<std::pair<Natron::Point,double> > >* /*strokes*/, int* /*strokeIndex*/) const {}

    virtual int getStrokeLastIndex() const { return -1; }
    
    virtual void getRenderStrokeData(RectD* /*lastStrokeMovementBbox*/, std::list<std::pair<Natron::Point,double> >* /*lastStrokeMovementPoints*/,
                                     double */*distNextIn*/, boost::shared_ptr<Natron::Image>* /*strokeImage*/) const {}
    
    virtual void updateStrokeImage(const boost::shared_ptr<Natron::Image>& /*image*/, double /*distNextOut*/, bool /*setDistNextOut*/) {}
    
    virtual RectD getLastPaintStrokeBbox() const { return RectD(); }
    
    virtual RectD getPaintStrokeWholeBbox() const { return RectD(); }
    
public Q_SLOTS:
    
    void quit();

    virtual void redrawAllViewers() {}

    void triggerAutoSave();

    void clearOpenFXPluginsCaches();

    void clearAllLastRenderedImages();

    void newVersionCheckDownloaded();

    void newVersionCheckError();

Q_SIGNALS:

    void pluginsPopulated();

protected:
    
    virtual void onGroupCreationFinished(const boost::shared_ptr<Natron::Node>& node, bool requestedByLoad, bool userEdited);

    virtual void createNodeGui(const boost::shared_ptr<Natron::Node>& /*node*/,
                               const boost::shared_ptr<Natron::Node>&  /*parentmultiinstance*/,
                               bool /*loadRequest*/,
                               bool /*autoConnect*/,
                               bool /*userEdited*/,
                               double /*xPosHint*/,
                               double /*yPosHint*/,
                               bool /*pushUndoRedoCommand*/)
    {
    }
    
private:
    
    
    void getWritersWorkForCL(const CLArgs& cl,std::list<AppInstance::RenderRequest>& requests);


    boost::shared_ptr<Natron::Node> createNodeInternal(const QString & pluginID,const std::string & multiInstanceParentName,
                                                       int majorVersion,int minorVersion,
                                                       bool requestedByLoad,
                                                       const NodeSerialization & serialization,bool dontLoadName,
                                                       bool autoConnect,double xPosHint,double yPosHint,
                                                       bool pushUndoRedoCommand,bool addToProject,bool userEdited,
                                                       bool createGui,
                                                       const QString& fixedName,
                                                       const CreateNodeArgs::DefaultValuesList& paramValues,
                                                       const boost::shared_ptr<NodeCollection>& group);
    
    void setGroupLabelIDAndVersion(const boost::shared_ptr<Natron::Node>& node,
                                   const QString& pythonModulePath,
                                   const QString &pythonModule);
    
    boost::shared_ptr<Natron::Node> createNodeFromPythonModule(Natron::Plugin* plugin,
                                                               const boost::shared_ptr<NodeCollection>& group,
                                                               bool requestedByLoad,
                                                               bool userEdited,
                                                               const NodeSerialization & serialization);
    
    boost::scoped_ptr<AppInstancePrivate> _imp;
};

class CreatingNodeTreeFlag_RAII
{
    AppInstance* _app;
public:
    
    CreatingNodeTreeFlag_RAII(AppInstance* app)
    : _app(app)
    {
        app->setIsCreatingNodeTree(true);
    }
    
    ~CreatingNodeTreeFlag_RAII()
    {
        _app->setIsCreatingNodeTree(false);
    }
};

#endif // APPINSTANCE_H
