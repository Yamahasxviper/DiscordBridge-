# Satisfactory Modding Resources

This document lists all available **FactoryGame modules**, **FactoryGame plugins**, **Engine plugins**, and **Engine runtime modules** that you can reference or depend on when building custom mods.

> **Note:** This document is auto-generated from the repository source. Plugins marked as disabled in the default project configuration can still be enabled by your mod's `.uproject` or `.uplugin` file.

---

## 1. FactoryGame Game Modules

These are the core game modules shipped with Satisfactory (FactoryGame). Your mod can declare dependencies on them.

| Module Name | Type | Loading Phase | Notes |
|---|---|---|---|
| FactoryGame | Runtime | Default | Depends on: Engine, CoreUObject, AIModule, GameplayTasks, SignificanceManager, ReplicationGraph, CinematicCamera, OnlineSubsystemUtils, UMG, DeveloperSettings, AbstractInstance, EnhancedInput, Foliage |
| FactoryPreEarlyLoadingScreen | Runtime | PreEarlyLoadingScreen |  |
| FactoryDedicatedServer | Runtime | Default | Only for: Server, Editor |
| FactoryDedicatedClient | Runtime | Default | Only for: Editor, Client, Game |

---

## 2. FactoryGame Custom Plugins

These plugins are developed by Coffee Stain Studios specifically for Satisfactory. They are available as plugin dependencies for your mods.

### AbstractInstance
*Abstraction layer for instancing reducing memory footprint and object count.*

**Modules:** `AbstractInstance`

### GameplayEvents
**Modules:** `GameplayEvents`

### InstancedSplines
*Instanced Splines*

**Modules:** `InstancedSplinesComponent`

### OnlineIntegration
*Utility plugin that can be used to enable Online Services Integration*

**Modules:** `OnlineIntegration`, `OnlineIntegrationEditor`, `OnlineIntegrationEOSExtensions`

### ReliableMessaging
*A plugin that enables reliable replication of bulk data via a TCP socket*

**Modules:** `ReliableMessaging`, `ReliableMessagingEOSP2P`, `ReliableMessagingSteamP2P`, `ReliableMessagingTCP`

### SignificanceISPC
*ISPC accelerated significance manager*

**Modules:** `SignificanceISPC`

---

## 3. Engine Plugins Enabled in FactoryGame

These engine plugins are explicitly enabled in `FactoryGame.uproject`. They are always available.

| Plugin Name | Notes |
|---|---|
| AbstractInstance |  |
| AnimationWarping |  |
| ApexDestruction |  |
| AppleProResMedia | Platforms: Win64 |
| BlueprintStats |  |
| ChaosVehiclesPlugin |  |
| ControlFlows |  |
| ControlRig |  |
| DTLSHandlerComponent |  |
| EditorScriptingUtilities |  |
| EditorTests |  |
| EnhancedInput |  |
| FbxAutomationTestBuilder |  |
| FullBodyIK |  |
| FunctionalTestingEditor |  |
| GameplayCameras |  |
| GameplayEvents |  |
| Gauntlet |  |
| GeometryCollectionPlugin |  |
| GeometryScripting |  |
| HairStrands |  |
| Landmass |  |
| LensDistortion |  |
| MfMedia |  |
| ModelViewViewModel |  |
| MovieRenderPipeline | Disabled for: Server |
| OnlineServicesNull |  |
| PythonScriptPlugin |  |
| RawInput |  |
| ReplicationGraph |  |
| RuntimeTests |  |
| SignificanceManager |  |
| Takes | Only for: Editor, Program |
| TemplateSequence |  |
| TestFramework |  |
| WinDualShock | Platforms: Win64 |
| WindowsDeviceProfileSelector |  |

---

## 4. All Available Engine Plugins (by Category)

The following engine plugins are available in this Unreal Engine build. To use them in your mod, add the plugin name to your `.uplugin` file's `Plugins` array.

### 2D

- **Paper2D**: Paper2D adds tools and assets to help create 2D games including animated sprite assets, tilesets (experimental), 2D level editing tools, and more.

### Ai

- **AISupport**: A simple plugin that makes sure your project loads AIModule and NavigationSystem at runtime
- **EnvironmentQueryEditor**: Allows editing of Environment Query assets, which are used by the AI to collect data about the environment/world
- **HTNPlanner**: [EXPERIMENTAL] Adds experimental support for Hierarchical Task Network (HTN) planner to the UE4's AI module
- **MassAI**: AI-specific functionality extenting MassGameplay
- **MassCrowd**: Spline based AI crowd system
- **MLAdapter**: A framework for training and utilizing machine learning agents in games. Creates an RPC interface through which an external process can query game state and control in-game actors. Once trained, agents can be run in-engine via neural networks loaded from ONNX models.

### Animation

- **ACLPlugin**: Use the Animation Compression Library (ACL) to compress AnimSequences.
- **AnimationData**: Animation Data
- **AnimationLocomotionLibrary**: Collection of techniques for driving locomotion animations
- **AnimationModifierLibrary**: Collection of Animation Modifiers
- **AnimationWarping**: Framework for animation and pose warping. This plugin includes Stride, Orientation, and Slope Warping alongside the Root Motion Delta animation attribute.
- **BlendSpaceMotionAnalysis**: Allows analysis of locomotion/root motion properties in blend spaces
- **ChaosClothGenerator**: Chaos Cloth Data Generator for ML Deformer
- **ControlRig**: Framework for animation driven by user controls.
- **ControlRigSpline**: Allows creation and use of splines for Control Rig
- **DeformerGraph**: Editor for creating GPU mesh deformation graphs
- **GameplayInsights**: Allows debugging of animation systems via Unreal Insights
- **IKRig**
- **LiveLink**: LiveLink allows streaming of animated data into Unreal Engine
- **LiveLinkCurveDebugUI**: Allows Viewing LiveLink Curve Debug Information
- **MLDeformerFramework**: Machine Learning Mesh Deformer Framework
- **MotionWarping**
- **NearestNeighborModel**: Nearest Neighbor Model for the ML Deformer Framework
- **NeuralMorphModel**: Neural Morph Model for the ML Deformer Framework
- **RigLogic**: 3Lateral RigLogic Plugin for Facial Animation v9.1.3
- **VertexDeltaModel**: Vertex Delta Model for the ML Deformer Framework

### Audiogameplay

- **AudioGameplay**: Core plugin for audio gameplay

### Audiogameplayvolume

- **AudioGameplayVolume**: Audio Gameplay Volume Plugin

### Blueprintfileutils

- **BlueprintFileUtils**: A Blueprint library that enables low-level file operations such as Move, Copy, Delete, and Find.

### Bridge

- **Bridge**: Megascans Link for Quixel Bridge.

### Cameras

- **CameraShakePreviewer**: Adds a new panel, accessible from the Level Editor, which lets the user preview camera shakes in editor viewports.
- **GameplayCameras**: Default gameplay camera classes and systems

### Compositing

- **Composure**: Plugin design to make compositing easier in Unreal Engine.
- **LensDistortion**: This plugin has been deprecated and will be removed in a future engine version. Please update your project to use the features of the CameraCalibration plugin instead. Plugin to generate UV displacement for lens distortion/undistortion on the GPU from standard camera model.
- **OpenColorIO**: Provides support for OpenColorIO
- **OpenCVLensDistortion**: Plugin to handle camera calibration and lens distortion/undistortion displacement map generation using OpenCV.

### Compression

- **OodleNetwork**: Oodle Network plugin for packet compression.

### Developer

- **AnimationSharing**: Plugin to create Shared Animation systems using the Leader-Follower pose functionality
- **BlankPlugin**: An example of a minimal plugin. This can be used as a starting point when creating your own plugin.
- **CLionSourceCodeAccess**: Allows access to source code in CLion.
- **CodeLiteSourceCodeAccess**: Allows access to source code in CodeLite.
- **ConcertClientSharedSlate**: Contains UI that is shared by client UI modules only
- **ConcertMain**: Allow collaborative multi-users sessions in the Editor
- **ConcertSharedSlate**: Contains UI that is shared for server and client UI modules
- **ConcertSyncClient**: Client plugin to enables multi-users editor sessions when connecting to a Concert Server
- **ConcertSyncCore**: Shared plugin for Concert Sync client and server plugins
- **ConcertSyncServer**: Server plugin to enables multi-users editor sessions
- **ConcertSyncTest**: Plugin to enables multi-users tests
- **DisasterRecoveryClient**: Track changes in the Editor to allow recovery in the event of a crash
- **DumpGPUServices**: Implements automatic upload services for the DumpGPU command.
- **GitSourceControl**: Git source control management
- **KDevelopSourceCodeAccess**: Allows access to source code in KDevelop.
- **MultiUserClient**: Allow collaborative multi-users sessions in the Editor
- **MultiUserServer**: Visualizes the multi-user server
- **N10XSourceCodeAccess**: Allows access to source code in the 10X Editor .
- **NullSourceCodeAccess**: Allows access to c++ projects while only looking for clang++
- **OneSkyLocalizationService**: OneSky localization service
- **PerforceSourceControl**: Perforce source control management
- **PixWinPlugin**: PIX for Windows graphics debugger integration.
- **PlasticSourceControl**: Plastic source control management
- **PluginUtils**: Helpers to create and edit plugins. Used by Plugin Browser.
- **PropertyAccessNode**: Blueprint node that allows access to properties via a property path
- **RenderDocPlugin**: RenderDoc graphics debugger/profiler integration.
- **RiderSourceCodeAccess**: Allows access to source code in Rider.
- **SubversionSourceControl**: Subversion source control management
- **TextureFormatOodle**: Oodle Texture plugin
- **TraceDataFilters**: Allows for turning on/off individual or sets of Trace Channels.
- **TraceSourceFilters**: Source data filtering for Unreal Insights.
- **UObjectPlugin**: An example of a plugin which declares its own UObject type. This can be used as a starting point when creating your own plugin.
- **VisualStudioCodeSourceCodeAccess**: Allows access to source code in Visual Studio Code.
- **VisualStudioSourceCodeAccess**
- **XcodeGPUDebuggerPlugin**: Xcode GPU debugger integration.
- **XCodeSourceCodeAccess**: Allows access to source code in XCode.

### Editor

- **AssetManagerEditor**: Editor UI and utilities for managing and auditing Assets on disk
- **AssetReferenceRestrictions**: Apply project-specific restrictions to how content in different folders or plugins can be referenced
- **AssetRegistryExport**
- **AssetSearch**
- **BlueprintHeaderView**: A tool to help convert Blueprint Classes to Native C++.
- **BlueprintMaterialTextureNodes**: Adds blueprint editor-only nodes for reading textures and render targets as well as creating and modifiying Material Instance Constants
- **ChangelistReview**: Review source control changelists
- **ConsoleVariables**: Save, load and control Console Variables (cvars) from this panel using Slate.
- **ContentBrowserAliasDataSource**: Data Source plugin providing allowing Content Browser items to appear in other directories other than their original location
- **ContentBrowserAssetDataSource**: Data Source plugin providing Asset Data to the Content Browser
- **ContentBrowserClassDataSource**: Data Source plugin providing Class Data to the Content Browser
- **ContentBrowserFileDataSource**: Data Source plugin providing loose file support for the Content Browser
- **CryptoKeys**
- **CurveEditorTools**: This provides a default set of editing tools for the Curve Editor.
- **DataValidation**: Editor UI and utilities for running data validation
- **DisplayClusterLaunch**: Launch local nDisplay nodes with ease.
- **EditorDebugTools**
- **EditorPerformanceModule**
- **EditorScriptingUtilities**: Helper functions to script your own UE editor functionalities with Blueprint or other scripting tools.
- **EngineAssetDefinitions**
- **FacialAnimation**: Bulk importer for facial animation curves and audio. Imports facial animation curve tables (from FBX) into sound waves.
- **GameplayTagsEditor**: GameplayTagsEditor provides blueprint nodes and editor UI to enable the use of GameplayTags for tagging assets and objects
- **GeometryMode**: Geometry and BSP editing
- **GuidedTutorials**: Adds classes and content that support running guided tutorials within the editor UI.
- **LightMixer**: Edit any properties of scene lights in a spreadsheet format!
- **LiveUpdateForSlate**: Refreshes the editor layout and tabs when Live Coding is complete
- **MacGraphicsSwitching**: Provides support for switching between multiple graphics devices on macOS.
- **MakeCookedEditorAsDLC**: This plugin is used in conjunction with the MakeCookedEditor UAT script to generate a cooked editor as a DLC add-on to a cooked client. It does not need to be enabled.
- **MaterialAnalyzer**: Analyzer to discover possible memory savings in material shaders.
- **MobileLauncherProfileWizard**: Wizard for mobile packaging scenarios
- **ModelingToolsEditorMode**: Modeling Tools Mode includes a suite of interactive tools for creating and editing meshes in the Editor
- **ObjectMixer**: Edit any properties of scene objects in a spreadsheet format!
- **PluginBrowser**: User interface for managing installed plugins and creating new ones.
- **PortableObjectFileDataSource**: Data Source plugin providing portable object (PO) file support for the Content Browser
- **SequencerAnimTools**: Animation Tools For Sequencer and Control Rig
- **SpeedTreeImporter**: An importer for SpeedTree runtime files.
- **StylusInput**: Support for advanced stylus and tablet inputs such as pressure, stylus & tablet buttons, and pen angles.
- **UVEditor**: Asset editor for modifying the UV mapping of a mesh
- **WaveformEditor**: Editor tool for waveforms
- **WorldPartitionHLODUtilities**: Editor utility classes & HLOD layer asset types

### Enhancedinput

- **EnhancedInput**: Input handling that allows for contextual and dynamic mappings.

### Enterprise

- **AxFImporter**: Importer for AxF material files.
- **DataprepEditor**: A tool to simplify creation and execution of data preparation pipelines from within the Unreal Editor.
- **DatasmithC4DImporter**: Adds support for importing content from Cinema4D applications into Unreal Engine
- **DatasmithCADImporter**: Collection of tools to work with CAD files.
- **DatasmithContent**: Content for Datasmith Importer.
- **DatasmithFBXImporter**: Adds support for importing content from DeltaGen and VRED into Unreal Engine
- **DatasmithImporter**: Importer for Datasmith files.
- **GLTFExporter**: An exporter for Khronos glTF 2.0.
- **LidarPointCloud**: Adds support for importing, processing and rendering of LiDAR Point Clouds.
- **MDLImporter**: Importer for MDL material files.
- **VariantManager**
- **VariantManagerContent**: Data classes and assets for the Variant Manager plugin

### Experimental

- **AbilitySystemGameFeatureActions**: Game feature actions to support modular use of the gameplay abilities system
- **ActorPalette**: Allows creation of Actor Palettes based on existing levels to quickly select actors and drag them into the level editor
- **AMFCodecs**
- **AnimNext**: Framework for defining functional data flow for animation systems
- **AnimToTexture**: Converts SkeletalMesh Animations into Textures
- **AppleVision**: Allows you to issue computer vision api calls for textures (camera data or render targets)
- **AssetPlacementEdMode**: The asset placement editor mode enables quick placement of assets into a world as full actors or various types of lighter weight objects, like batched render instances.
- **AutomationUtils**: Tools and Utilities for Automation purposes
- **AVCodecsCore**: Core Plugin for various Audio/Video codecs
- **BackChannel**: BackChannel is an experimental plugin that allows external tools and apps to query for and push data into a running Unreal session.
- **BaseCharacterFXEditor**: Base classes for character FX asset editors
- **BlueprintSnapNodes**: Prototype of an alternative way to lay out Blueprint nodes in a more compact manner
- **BlueprintStats**: Blueprint Stats
- **ChaosCaching**: Chaos Cache asset support for recording and playing back physics simulations
- **ChaosCloth**: Adds Chaos Cloth Module.
- **ChaosClothAsset**: Pattern based cloth asset using the Chaos Cloth simulation.
- **ChaosClothAssetEditor**: Editor for modifying cloth assets
- **ChaosClothEditor**: Editor module accompanying the Chaos Cloth runtime module.
- **ChaosEditor**: Destruction Tools
- **ChaosFlesh**: Chaos Flesh Simulation
- **ChaosNiagara**: Import destruction data from Chaos into Niagara to generate secondary destruction effects.
- **ChaosSolverPlugin**
- **ChaosUserDataPT**: Custom per-particle userdata. Write-only on game thread, read-only on physics thread.
- **ChaosVD**: Enables support for Visual debugging of Chaos Physics simulations
- **ChaosVehiclesPlugin**: Chaos Vehicle Integration
- **CharacterAI**: Adds code and assets related to implementing AI in a character-based project.
- **Chooser**: Chooser
- **CineCameraRigs**: Extended camera rigs for cinematic workflow
- **CineCameraSceneCapture**: This plugin adds the ability to render cine camera views into Render Targets identically to Scene Capture
- **CinematicPrestreaming**: Adds a way to record certain types of streaming data requests in cinematic cutscenes. The requests can then be played back in advance on the Sequencer timeline to pre-stream data during normal gameplay/rendering.
- **CodeEditor**: [EXPERIMENTAL] Allows editing of code from within the Unreal editor
- **CodeView**: Provides an in-editor code view of game classes and structures with direct IDE accessibility
- **ColorCorrectRegions**: Color correction/shading constrained to regions/volumes
- **CommonConversation**: An *experimental* plugin for authoring graph-based conversation trees
- **ContextualAnimation**
- **ControlFlows**: Tool to cleanly implement Asynchronous Operations
- **CurveExpression**: Experimental Curve Remapper using Simple Math Expressions
- **Dataflow**: Editor Dataflow Graph
- **DataprepGeometryOperations**: Experimental geometry processing operations usable in the Dataprep Editor.
- **DatasmithCloTranslator**
- **DatasmithInterchange**: Interchange Importer for Datasmith.
- **DatasmithRuntime**
- **DefaultInstallBundleManager**
- **ExampleCharacterFXEditor**: Example asset editor using the BaseCharacterFXEditor base classes
- **FieldSystemPlugin**
- **Fracture**: Adds Module for FractureEditor
- **FullBodyIK**
- **GameFeatures**: Support for modular Game Feature Plugins
- **GameplayBehaviors**: Encapsulated fire-and-forget behaviors for AI agents
- **GameplayGraph**: A graph representation model and common graph alogrithms that can be used for gameplay.
- **Gauntlet**: Provides a helper class for creating and managing tests in your game
- **GeometryCacheAbcFile**: Support Geometry Cache from Alembic file without importing
- **GeometryCollectionPlugin**: Adds Geometry Collection Container.
- **GeometryFlow**: Geometry DataFlow Graph
- **GeometryScripting**: Geometry Script provides a library of functions for creating and editing Meshes in Blueprints and Python
- **GizmoEdMode**: Editor mode to manage InteractiveToolFramework based global TRS gizmos
- **GizmoFramework**: Toggle use of InteractiveToolsFramework based TRS Gizmos
- **GPULightmass**: Static lighting building & previewing system using DXR
- **HairModelingToolset**
- **ImagePlate**: Actor and component types that provide a camera-aligned image plate
- **ImpostorBaker**: Generates a variety of Impostors for use as distant mesh LODs.
- **Iris**: Iris networking.
- **JWT**: An API for working with JSON Web Token (JWT) data.
- **Landmass**
- **LandscapePatch**: Support for adding landscape patches- components that can be attached to meshes to affect the landscape as the mesh is repositioned.
- **LearningAgents**: Learning Agents is a machine learning library for AI character control in games. It simplifies the use of reinforcement and imitation learning in Unreal.
- **LedWallCalibration**: Tools for Led Wall calibration
- **LiveLinkControlRig**: Allows access to LiveLink Data through Control Rig
- **LiveLinkFaceImporter**: Imports CSV recordings from the Live Link Face app.
- **LocalizableMessage**
- **MeshLODToolset**
- **MeshModelingToolsetExp**: A set of experimental modules implementing 3D mesh creation and editing based on the Interactive Tools Framework
- **ModularGameplay**: Base classes and subsystems to support modular use of the gameplay framework
- **MotionTrajectory**: Generate predictions and track history of character motion.
- **MotorSimOutputMotoSynth**: A MotorSim Output component using MotoSynth.
- **MotoSynth**: An experimental granular vehicle engine. Intended to explore and demonstrate potential capabilities. Not supported.
- **Mutable**: Mutable adds the tools and runtime to create customizable objects for your games.
- **NaniteDisplacedMesh**: Asset and component types that provide a basic pre-displacement pipeline for Nanite meshes
- **NNE**: Framework for running deep neural networks and other machine learning inference in Unreal Engine. This replaces the Neural Network Inference (NNI) plugin.
- **NNERuntimeORTCpu**: CPU accelerated runtime for the NNE plugin (deep learning and neural network inference in Unreal Engine) backed by ONNX Runtime CPU providers.
- **NNERuntimeORTGpu**: GPU accelerated runtime for the NNE plugin (deep learning and neural network inference in Unreal Engine) backed by ONNX Runtime Dml and Cuda providers.
- **NNERuntimeRDG**: GPU RDG accelerated runtime for the NNE plugin (deep learning and neural network inference in Unreal Engine) backed by Unreal rendergraph RDG.
- **NVCodecs**: Adds codecs from the NVIDIA Media Codec SDK to AVCodecs
- **OpenImageDenoise**: Denoising engine for the Unreal Path Tracer based on Intel's OpenImageDenoise library.
- **Optimus**: Deprecated plugin now redirected to DeformerGraph
- **OptiXDenoise**: Denoising engine for the Unreal Path Tracer based on NVIDIA's OptiX AI-Accelerated Denoiser library.
- **PanoramicCapture**: A plugin to capture a sequence of panoramic images in monoscopic or stereoscopic (top/bottom).
- **PCG**: Visual scripting framework for procedurally populating worlds with content in editor and/or at run-time.
- **PCGExternalDataInterop**
- **PCGGeometryScriptInterop**: Extra plugin for Procedural Content Generation Framework interacting with Geometry Scripts.
- **PhysicsControl**: Additional Support for controlling static and skeletal meshes through physical controls
- **PixelStreamingPlayer**: Support for receiving a pixel streaming stream and displaying it in game.
- **PlanarCut**: Adds Module for Planar Cuts.
- **PlatformCrypto**: Exposes a unified API for cryptography functionality provided by the platform, if available. Otherwise, interfaces with OpenSSL.
- **PluginAudit**: Editor plugin for auditing plugin connectivity.
- **PluginReferenceViewer**: Editor plugin for viewing plugin references.
- **PluginTemplateTool**
- **PointCloud**: An efficient way to render point cloud data that is generated by AR devices
- **PoseSearch**: Experimental pose search API
- **ProxyLODPlugin**: A plugin to generate Proxy LOD systems.
- **PythonFoundationPackages**: Common Python packages such as NumPy and PyTorch used by engine plugins
- **PythonScriptPlugin**: Python integration for the Unreal Editor.
- **QuicMessaging**: Adds a QUIC based transport layer to the messaging sub-system for sending and receiving messages between networked computers and devices.
- **RawInput**: RawInput provides an interface to receive input from Flight Sticks, Steering Wheels, and other non-XInput supported devices in Windows.
- **RemoteSession**: A plugin for Unreal that allows one instance to act as a thin-client (rendering and input) to a second instance
- **RenderGrid**: Advanced pipeline for use in creating rendered cinematics.
- **SampleToolsEditorMode**: Sample Tools Mode includes a set of sample Tools demonstrating capabilities of the Interactive Tools Framework
- **ScreenReader**: A plugin that contains accessibility classes and frameworks that can be extended to offer vision accessibility services.
- **ScriptableToolsEditorMode**: Editor Mode for Scriptable Tools
- **ScriptableToolsFramework**: Blueprint-Scriptable extension to the Interactive Tools Framework
- **Shotgrid**: ShotGrid integration for the Unreal Editor.
- **SimpleHMD**: SimpleHMD is a sample of a basic stereo HMD implementation
- **SkeletalMeshModelingTools**
- **SkeletalReduction**: A plugin to generate LOD for deforming meshes.
- **SlateScreenReader**: A screen reader that provides vision accessibility services for Slate.
- **StaticMeshEditorModeling**
- **StructUtils**: Experimental Struct Utilities supplying InstancedStruct type
- **TargetingSystem**: Generic targeting system for use with gameplay abilities, aim assist, etc
- **TetMeshing**: Adds Module for Generating and Refining Tetrahedral Meshes.
- **Text3D**: 3D Text Generator
- **TextToSpeech**: A text to speech system that can be used to make auditory speech announcements given input strings.
- **TextureMediaPlayer**
- **ToolPresets**: Adds support for saving and loading tool settings as presets.
- **TypedElementsDataStorage**: A central extendable data storage for editors and their corresponding data with support for viewing and editing through a collection of widgets.
- **UIFramework**: A framework to control UMG from server.
- **UserToolBoxBasicCommand**: Basic set of command to populate a custom editor tab
- **UserToolBoxCore**: Core functionnality to create custom editor tab
- **VirtualCamera**: Content for VirtualCameraCore which adds actors, components, and utilities for controlling and viewing cameras via physical devices.
- **VirtualCameraCore**: Code for actors, components, and utilities for controlling and viewing cameras via physical devices. See VirtualCamera for content.
- **VirtualHeightfieldMesh**: Mesh renderer for virtual texture heightfields
- **VirtualProductionUtilities**: Utility classes and functions for Virtual Production
- **VirtualScouting**: Virtual Scouting lets filmmakers scout a digital environment in virtual reality.
- **Volumetrics**: A library of volume creation and rendering tools using Blueprints.
- **VPRoles**
- **VPSettings**
- **Water**: Full suite of water tools and rendering techniques to easily add oceans, river, lakes or custom water bodies that carve landscape and interacts with gameplay
- **WaterExtras**: Samples, test maps, etc intended to help developers start using the water system. Not intended to be used directly in a shipping product.
- **WaveFunctionCollapse**: Wave Function Collapse tools for tile-based model synthesis
- **WebAPI**: Automated generation of web based APIs
- **WebSocketNetworking**: WebSocket Networking - NOTE: MUST disable all other existing NetDriverDefinitions in order to use WebSocketNetDriver. ALSO: MUST disable all PackHandlerComponents not supported by HTML5/Websockets (e.g. SteamAuthComponentModuleInterface)
- **WidgetEditorToolPalette**: A set of tools to enhance UMG creation UX
- **WMFCodecs**: Adds codecs from the Windows Media Foundation to AVCodecs
- **XRCreativeFramework**

### Fab

- **Fab**: Pre-release Fab Plugin

### Fastbuildcontroller

- **FastBuildController**

### Fx

- **CascadeToNiagaraConverter**: Add support for scriptable conversion of Cascade Systems to Niagara Systems.
- **ExampleCustomDataInterface**: This plugin contains C++ example content that shows how to write your own data interface for Niagara. Check out the plugin folder Engine/Plugins/FX/ExampleCustomDataInterface for the source files.
- **Niagara**: Niagara effect systems.
- **NiagaraFluids**: Fluid simulation toolkit for Niagara
- **NiagaraSimCaching**: Adds support for recording and playing back Niagara simulations in sequencer via take recorder

### Importers

- **AlembicHairImporter**: Import Hair Strands from Alembic file
- **AlembicImporter**: Support importing Alembic files
- **USDImporter**: Adds support for importing the USD file format into Unreal Engine
- **USDMultiUser**: Enables opt-in multi-user synchronization for the USD Importer plugin.

### Interchange

- **Interchange**: Interchange framework plugin implement all engine importers and exporters.
- **InterchangeEditor**: Interchange Editor plugin pull on Interchange plugin to allow us to force interchange plugin in editor.

### Jsonblueprintutilities

- **JsonBlueprintUtilities**: Json functionality for Blueprint.

### Lightweightinstanceseditor

- **LightWeightInstancesEditor**: Light Weight Instances provide the flexibility and interaction of actors while having performance similar to instanced meshes.

### Media

- **AjaMedia**: Implements input and output using AJA Capture cards.
- **AndroidCamera**: Implements camera preview using the Android Camera library.
- **AndroidMedia**: Implements a media player using the Android Media library.
- **AppleProResDecoderElectra**: Implements video playback of Apple ProRes encoded videos. Apple ProRes is a high quality, lossy video compression format.
- **AppleProResMedia**: Implements video playback and the export of the Apple ProRes Codec. Apple ProRes is a high quality, lossy video compression format.
- **AudioCaptureTimecodeProvider**: Decodes an LTC signal (linear timcode) from a live audio capture device (ie. the computer audio jack).
- **AvfMedia**: Implements a media player using Apple AV Foundation.
- **AvidDNxHDDecoderElectra**: Avid DNxHD® video decoder
- **AvidDNxMedia**: Implements video export using Avid DNx Codecs.
- **BinkMedia**: Implements a media player using Bink.
- **BlackmagicMedia**: Implements input and output using Blackmagic Capture cards.
- **ElectraCDM**
- **ElectraCodecs**: Codecs for use with Electra player.
- **ElectraPlayer**: Next-generation playback capability.
- **ElectraSubtitles**: Subtitle Decoder Module for Electra Player Media Playback
- **ElectraUtil**: Reusable Base Components for Electra Player Media Playback
- **HAPDecoderElectra**: Implements video playback of the HAP Codec. HAP is a high performance, high resolution codec that runs on the GPU.
- **HAPMedia**: Implements video playback of the HAP Codec. HAP is a high performance, high resolution codec that runs on the GPU.
- **HardwareEncoders**
- **ImgMedia**
- **LinearTimecode**: Component to read a linear timecode from a media source. Does not use synchronization mechanism.
- **MediaCompositing**: Actors, components and Sequencer extensions for compositing media
- **MediaFrameworkUtilities**: Utility assets and actors to ease the use of the Media Framework.
- **MediaIOFramework**: Media Framework classes to support Professional Media IO used by the Virtual Production industry.
- **MediaMovieStreamer**: Movie Streamer using MediaFramework.
- **MediaPlate**
- **MediaPlayerEditor**: Content Editor for MediaPlayer Assets.
- **MfMedia**: Implements a media player using the Microsoft Media Foundation framework. Requires Xbox One or Windows 7 and higher.
- **OpusDecoderElectra**: Implements Opus audio playback with the Electra media player
- **PixelCapture**: Framework for capturing pixel buffers in other formats while allowing for disconnected produce/consume rates.
- **PixelStreaming**: Streaming of Unreal Engine audio and rendering to WebRTC-compatible media players such as a web browsers.
- **TimecodeSynchronizer**: This plugin has been deprecated and will be removed in a future engine version. Please update your project to use the features of the TimedDataMonitor plugin instead. An asset that will become the TimecodeProvider once all the inputs get synchronized to a timecode.
- **VPxDecoderElectra**: Implements VP8 and VP9 playback with the Electra media player on desktop machines
- **WebMMedia**
- **WmfMedia**: Implements a media player using the Windows Media Foundation framework.

### Memoryusagequeries

- **MemoryUsageQueries**: Memory Usage Queries, original contribution from The Coalition (Microsoft) https://thecoalitionstudio.com

### Meshpainting

- **MeshPainting**: System for painting data onto meshes.

### Messaging

- **MessagingDebugger**: Provides a visual debugger for the messaging sub-system.
- **TcpMessaging**: Adds a TCP connection based transport layer to the messaging sub-system for sending and receiving messages between networked computers and devices.
- **UdpMessaging**: Adds a UDP based transport and tunneling layer to the messaging sub-system for sending and receiving messages between networked computers and devices.

### Moviescene

- **ActorSequence**: Runtime for embedded actor sequences
- **CustomizableSequencerTracks**: Library that provides a blueprintable track type that can be added to sequencer
- **LevelSequenceEditor**: Content Editor for LevelSequence Assets.
- **MoviePipelineMaskRenderPass**: Additional render passes for the Movie Render Queue. This currently includes the ObjectId pass (Editor Only) which generates object mattes with some limitations (using the Cryptomatte specification), and a Panoramic pass with better Sequencer integration than the Panoramic Capture plugin.
- **MovieRenderPipeline**: Advanced movie rendering pipeline for use in creating rendered cinematics or other multi-media creation.
- **MovieSceneTextTrack**: Adds support to key Text Properties in Movie Scene Sequences
- **ReplayTracks**: Sequence tracks for playing recorded gameplay
- **SequencerScripting**
- **TemplateSequence**: Runtime for template sequences

### Netcodeunittest

- **NetcodeUnitTest**: A unit testing framework for testing the Unreal Engine netcode, primarily for bugs and exploits
- **NUTUnrealEngine**: Exploit unit tests for Unreal Engine and some base Unreal Engine games, based on the Netcode Unit Test framework

### Online

- **AndroidFetchBackgroundDownload**: An Android plugin for enabling BackgroundHTTP requests to work while the app is backgrounded through use of the Fetch API.
- **EOSShared**: Responsible for init/shutdown of the EOSSDK runtime library.
- **EOSVoiceChat**: IVoiceChat integration of the EOS Voice service
- **OnlineBase**: Shared code online subsystem (OSSv1) and online service (OSSv2) interfaces.
- **OnlineFramework**: Shared code for interacting with online gameplay services.
- **OnlineServices**: Shared code for interacting with online services implementations.
- **OnlineServicesEOS**: Online Services implementation for EOS Account and Game services.
- **OnlineServicesEOSGS**: Online Services implementation for EOS Game services only.
- **OnlineServicesNull**: Online Services implementation without an external service.
- **OnlineServicesOSSAdapter**: Online Services adapter for Online Subsystem implementations.
- **OnlineSubsystem**: Shared code for interacting online subsystem implementations.
- **OnlineSubsystemAmazon**: Access to Amazon platform
- **OnlineSubsystemApple**
- **OnlineSubsystemEOS**: Online Subsystem for Epic Online Services
- **OnlineSubsystemFacebook**: Access to Facebook platform
- **OnlineSubsystemGoogle**: Access to Google platform
- **OnlineSubsystemGooglePlay**: Access to GooglePlay platform
- **OnlineSubsystemIOS**
- **OnlineSubsystemNull**: Access to NULL platform
- **OnlineSubsystemOculus**: Access to Oculus platform. This plugin is deprecated and will be removed in a future engine release.
- **OnlineSubsystemSteam**: Access to Steam platform
- **OnlineSubsystemTencent**: Access to Tencent platform
- **OnlineSubsystemUtils**: Shared code for interacting online service and online subsystem implementations.
- **SocketSubsystemEOS**: Responsible for management of EOS P2P Socket connections.
- **VoiceChat**: Voice Chat Interface
- **WebAuth**: Access to Web Authenticated Sessions.

### Performance

- **PerformanceMonitor**: A plugin for tracking the value of certain timers during gameplay.

### Portal

- **LauncherChunkInstaller**: Chunk installer module that hooks into launcher

### Protocols

- **MQTT**: MQTT broker and client

### Rendergraphinsights

- **RenderGraphInsights**: Allows debugging of RDG via Unreal Insights

### Runtime

- **ActorLayerUtilities**: Utilites for interacting with actor layers from blueprints
- **Adjust**: Adjust Analytics Provider
- **ADOSupport**: ADO (ActiveX Data Objects) Database Support
- **AESGCMHandlerComponent**
- **AESHandlerComponent**
- **AnalyticsBlueprintLibrary**: Blueprint Library for using an analytic event provider
- **AnalyticsMulticast**: Forwards analytics API calls to a list of analytics providers to log data to multiple services at once
- **AndroidBackgroundService**: Allows you to use AndroidX WorkManager to perform background work on Android
- **AndroidDeviceProfileSelector**: Android Device Profile Selector used show selection of device profiles on hardware
- **AndroidFileServer**: Adds support for remote file management to Android projects.
- **AndroidMoviePlayer**: Android Platform Movie Player using Android Media library
- **AndroidPermission**: Support for Android Runtime Permission
- **AnimationBudgetAllocator**: Constrains the time taken for animation to run by dynamically throttling skeletal mesh component ticking.
- **ApexDestruction**: APEX implementation of destruction
- **AppleARKit**: Support for Apple's ARKit augmented reality system
- **AppleARKitFaceSupport**: Support for Apple's face tracking features
- **AppleImageUtils**: Utilities that operate on CIImage, CVPixelBuffer, IOSurface, etc.
- **AppleMoviePlayer**: Apple Platform Movie Player using AVPlayer library
- **ArchVisCharacter**: A controllable character tuned for architectural applications
- **ARUtilities**: Utility code and content for AR systems
- **AssetTags**: Provides high-level management and access to asset tags and collections for runtime and editor scripting.
- **AudioCapture**: Plugin provides an interface for microphone input capture.
- **AudioModulation**: Default implementation of Audio Modulation in the Unreal Audio Engine.
- **AudioMotorSim**: Compositional method for simulating audio for vehicles.
- **AudioSynesthesia**: A variety of offline analyzers for integrating exposing extracted audio metadata to blueprints.
- **AudioWidgets**: Collection of widgets tailored to interacting with audio-related data and systems.
- **AzureSpatialAnchors**: Support for Microsoft Azure Spatial Anchor cloud service.
- **AzureSpatialAnchorsForARCore**: Microsoft Azure Spatial Anchors implementation for ARCore
- **AzureSpatialAnchorsForARKit**: Microsoft Azure Spatial Anchors implementation for ARKit
- **CableComponent**: A simulated cable component.
- **ChunkDownloader**: Implements a streaming install client
- **CommonUI**: A repository for game independent UI elements.
- **ComputeFramework**: Support for user authored GPU compute graphs
- **CustomMeshComponent**: A new renderable Component class that allows you to specify custom geometry via C++ or Blueprint.
- **DatabaseSupport**: Abstract Database Support
- **DataRegistry**: Adds Data Registry system that can be used as a generic interface for acquiring structure data from multiple sources at runtime
- **DTLSHandlerComponent**: Provides a packet handler component to do DTLS encryption and decryption.
- **DummyMeshReconstructor**: Sample of how to drive mesh reconstruction. Generates dummy geometry to demonstrate API usage.
- **ExampleDeviceProfileSelector**: Example Device Profile Selector used show selection of device profiles on hardware
- **FileLogging**: Writes analytic API calls to local disk for debugging or local use
- **Firebase**: Support for remote notifications using Firebase
- **Flurry**: Flurry Analytics Provider
- **GameplayAbilities**: Adds GameplayEffect and GameplayAbility classes to handle complicated gameplay interactions.
- **GameplayBehaviorSmartObjects**: Plugins for SmartObjects using GameplayBehavior as their default runtime behavior
- **GameplayInteractions**: Player and NPC interactions
- **GameplayStateTree**: StateTree for AI/Gameplay Behaviors
- **GeForceNOWWrapper**: NVidia GeForce NOW Wrapper
- **GeometryCache**: Support for distilled Geometry animations
- **GeometryProcessing**: Data Structures and Algorithms for Processing 2D and 3D Geometry
- **GeoReferencing**: GeoReferencing tools for UE worlds
- **GoogleARCore**
- **GoogleARCoreServices**
- **GoogleCloudMessaging**: Support for remote notifications using Google Cloud Messaging
- **GooglePAD**: Google Play Asset Delivery for Android.
- **HairStrands**: Rendering and simulation of grooms
- **HDRIBackdrop**
- **HPMotionController**: Controller mappings for the HP Reverb G2 motion controller in OpenXR and SteamVR
- **HTTPChunkInstaller**: Implements a streaming install client
- **InputDebugging**: Input debugging and visualization.
- **IOSDeviceProfileSelector**: IOS Device Profile Selector used show selection of device profiles on hardware
- **IOSReplayKit**: Support for local recording and broadcasting using ReplayKit
- **IOSTapJoy**: IOS TapJoy Provider
- **LevelStreamingPersistence**: Experimental Level Streaming Persistence framework
- **LinuxDeviceProfileSelector**: Linux Device Profile Selector
- **LiveLinkOverNDisplay**: LiveLink subjects synchronization for nDisplay setup
- **LocationServicesAndroidImpl**: Android implementation for blueprint access for location data from mobile devices
- **LocationServicesBPLibrary**: Common interface for blueprint access for location data from mobile devices
- **LocationServicesIOSImpl**: IOS implementation for blueprint access for location data from mobile devices
- **MassEntity**: Gameplay-focused framework supporting data-oriented processing
- **MassGameplay**: Implementation of large-scale agent simulation based on MassEntity
- **MeshModelingToolset**: A set of modules implementing 3D mesh creation and editing based on the Interactive Tools Framework
- **Metasound**: A high-performance audio system that enables sound designers to have complete control over audio DSP graph generation of sound sources, via sample-accurate control and modulation of sound using audio parameters and audio events from game data and Blueprints
- **MicroSoftSpatialSound**: Audio spatialization plugin using Microsoft's SASAPI service.
- **MIDIDevice**: Allows you to send and receive MIDI events through a simple API in either C++ or Blueprints
- **MixedRealityCaptureFramework**: A simple framework that provides users a way to integrate mixed reality capture into their VR projects.
- **MobileFSR**: Mobile FSR 1.0
- **MobilePatchingUtils**: Blueprint exposed functionality for downloading and patching content on mobile platforms
- **ModelViewViewModel**: A plugin to support the Model-View-Viewmodel pattern in UMG.
- **MsQuic**: Runtime plugin for the MsQuic library.
- **NavCorridor**: Experimental Navigation Corridor
- **nDisplay**: Support for synchronized clustered rendering using multiple PCs in mono or stereo
- **nDisplayModularFeatures**: Modular Features for nDisplay
- **NetworkPrediction**: Generalized framework for writing network prediction friendly gameplay systems
- **NetworkPredictionExtras**: Non essential classes for Network Prediction. Samples, test maps, etc intended to help developers start using the system. Not intended to be used directly in a shipping product.
- **NetworkPredictionInsights**: Allows debugging of NetworkPrediction via Unreal Insights
- **OculusAudio**: Oculus audio spatialization
- **OpenCV**: Plugin initializing OpenCV library to be used in engine.
- **OpenXR**: OpenXR is an open VR/AR standard
- **OpenXREyeTracker**: OpenXR Eye Tracker provides XR_EXT_eye_gaze_interaction support.
- **OpenXRHandTracking**: OpenXR Hand Tracking provides XR_EXT_hand_tracking support.
- **OpenXRMsftHandInteraction**: OpenXRMsftHandInteraction provides support for the XR_MSFT_hand_interaction OpenXR Extension. This allows hand tracking to act as a motion controller.
- **OpenXRViveTracker**: OpenXR Vive Tracker provides XR_HTCX_vive_tracker_interaction.
- **OptionalMobileFeaturesBPLibrary**: Gives blueprint access to Sound Volume, Battery Charge Level, and System Temperature for Android and iOS devices
- **OSC**: Implements the OSC 1.0 specification, allowing users to send and receive OSC messages and bundles between remote clients or applications.
- **OSCModulationMixing**: Utility Blueprint objects and functions enabling Control Modulation mix profiling using the OSC (Open Sound Control) protocol.
- **PreLoadScreenMoviePlayer**: Handles a default implementation of using a Pre-Load screen to dislplay an engine loading movie
- **ProceduralMeshComponent**: A renderable component and library of utilities for creating and modifying mesh geometry procedurally.
- **PropertyAccessEditor**: Editor support for copying properties from one object to another. Required for Animation and UMG systems to function correctly
- **Reflex**: NVIDIA Reflex Latency Tracking and Tick Rate Handling
- **RemoteDatabaseSupport**: Remote Database Support
- **RenderTrace**: The Render Trace plugin provides a way to have pixel perfect sampling of physical materials on meshes.
- **ReplicationGraph**: The Replication Graph plugin provides a Replication Driver implementation designed for a large number of actors and connections by mainting persistent replicated actor lists in a graph structure.
- **ReplicationSystemTestPlugin**: Unit and functional tests for the network replication system.
- **ResonanceAudio**
- **RigVM**: Provides frontend and backend for the RigVM visual programming language and runtime
- **SignificanceManager**: The significance manager plugin provides an extensible framework for allowing games to calculate the significance of an object and change behavior in response.
- **SkeletalMerging**: Provides Blueprint functionality to perform runtime Skeletal Mesh merging
- **SmartObjects**: Support for ambient life populating the game world
- **SoundCueTemplates**: Collection of SoundCue Templates, which provide rapid design of common audio design workflows.
- **SoundFields**: Plugin featuring a variety of basic audio SoundFields solutions.
- **SoundMod**: Supports playback of ProTracker (MOD), Scream Tracker 3 (S3M), Fast Tracker II (XM), and Impulse Tracker (IT) files.
- **Soundscape**: A Dynamic Ambient Sound System
- **SoundUtilities**: A variety of BP functions, objects, and utilities for audio.
- **Spatialization**: Plugin featuring a variety of basic audio spatialization solutions.
- **SQLiteCore**: SQLite Database
- **SQLiteSupport**: SQLite Database Support
- **StateTree**: General purpose hierarchical state machine
- **SteamAudio**: This plugin is deprecated and will be removed in a future engine release. Please use the plugin from Valve's website.
- **SteamController**: InputDevice plugin for Steam controller
- **SteamShared**: Shared module loader for the Steam API
- **SteamSockets**: New Steamworks Networking code that supports the SteamSockets interface. NOTE: This plugin is only compatible with the SteamSockets Netdriver. It will not work if the proper netdriver definitions have not been set.
- **SunPosition**: Calculates the sun position based on latitude/longitude and date/time.
- **Synthesis**: A variety of realtime synthesizers and DSP source and submix effects.
- **WarpUtils**: PFM/MPCDI generation & visualization
- **WaveTable**: Default implementation of WaveTable support within the Unreal Audio Engine.
- **WebBrowserNativeProxy**: Maintains the browser to native proxy and provides hooks for registering UObjects bindings
- **WebBrowserWidget**: Allows the user to create a Web Browser Widget
- **WebMMoviePlayer**: Movie Player for WebM files
- **WindowsDeviceProfileSelector**: Windows Device Profile Selector used to determine the system settings for windows platforms
- **WindowsMoviePlayer**: Windows Specific Movie Player using Media Foundation
- **WinDualShock**: InputDevice plugin for the PS4 DualShock controller in Windows
- **WorldConditions**: General purpose cached conditions
- **XRBase**: XR Base Feature Implementations. (Generally this plugin will be automatically enabled by another plugin that requires it.)
- **XRScribe**: OpenXR API Capture/Emulation
- **XRVisualization**: Visualization Library for XR HMDs and controllers
- **ZoneGraph**: Description missing.
- **ZoneGraphAnnotations**

### Scriptplugin

- **ScriptPlugin**: An example of a script plugin. This can be used as a starting point when creating your own plugin.

### Slate

- **SlateInsights**: Allows debugging of Slate via Unreal Insights
- **SlateScripting**: Allows interacting with Slate through scripting

### Tests

- **AutomationDriverTests**
- **CQTest**: Simplified testing classes for Unreal Engine
- **EditorTests**
- **FbxAutomationTestBuilder**
- **FunctionalTestingEditor**
- **InterchangeTests**: Plugin for Interchange automation tests.
- **PythonAutomationTest**
- **RHITests**
- **RuntimeTests**
- **TestFramework**
- **TestSamples**
- **WidgetAutomationTests**

### Traceutilities

- **TraceUtilities**

### Virtualproduction

- **CameraCalibration**: Framework to support lens distortion and camera calibration in engine.
- **CameraCalibrationCore**: Supports lens distortion and camera calibration.
- **CompositePlane**: Provides a cine camera actor for projecting textures and videos
- **DataCharts**: Generate charts based on data tables
- **DatasmithMVR**
- **DMXControlConsole**
- **DMXDisplayCluster**: Allows integration between DMX and DisplayCluster
- **DMXEngine**: Functionality and assets for communication with DigitalMultiplexer (DMX) enabled devices
- **DMXFixtures**: DMX Light Fixtures Blueprints
- **DMXModularFeatures**: Modular Features for DMX
- **DMXPixelMapping**: Tools set for map LED digital pixel strip or fixture arrays regardless of shape or size
- **DMXProtocol**: DMX Protocols implementation
- **EpicStageApp**: Enables remote connections from the Epic Stage App
- **ICVFX**: Conveniently collects plugins for In-Camera VFX
- **ICVFXTesting**: Testing utilities to be used by ICVFX projects
- **LevelSnapshots**
- **LiveLinkCamera**: Live Link plugin adding functionalities for camera handling
- **LiveLinkFreeD**: Live Link plugin for the FreeD protocol
- **LiveLinkLens**: Adds a new LiveLink LensRole and LensController to support streaming of pre-calibrated lens data
- **LiveLinkMasterLockit**: Live Link support for the Ambient MasterLockit metadata server
- **LiveLinkPrestonMDR**: Live Link support for the Preston MDR-3 Motor Driver
- **LiveLinkVRPN**: Live Link plugin for the VRPN protocol
- **LiveLinkXR**: Live Link plugin for using XR tracked devices
- **MultiUserTakes**: Enables opt-in multi-user synchronization for Take Recorder.
- **RemoteControl**: A suite of tools for controlling the Unreal Engine, both in Editor or at Runtime via a webserver. This allows users to control Unreal Engine remotely through HTTP or WebSockets requests. This functionality allows developers to control Unreal through 3rd party applications and web services.
- **RemoteControlInterception**: Plugin that allows to intercept Remote Control commands
- **RemoteControlProtocolDMX**: Allows interactions between DMX and RemoteControl API.
- **RemoteControlProtocolMIDI**: Allows interactions between MIDI and RemoteControl API.
- **RemoteControlProtocolOSC**: Allows interactions between OSC and RemoteControl API.
- **RemoteControlWebInterface**: Provides a web interface to control unreal engine via presets, requires nodejs to be installed
- **RivermaxCore**: Base plugin exposing rivermax to engine
- **RivermaxMedia**: Adding NVIDIA Rivermax capabilities for Media Captures and Media Players
- **RivermaxSync**: Adding NVIDIA Rivermax synchronization capabilities for nDisplay
- **SequencerPlaylists**: Sequencer Playlists allow users to prepare, queue, and trigger level sequences on the fly during a virtual production session, providing increased flexibility and agility when interacting with animation on set.
- **StageMonitoring**: Plugin enabling monitoring in the context of a virtual production stage where multiple machines are in operation
- **Switchboard**: Launcher/Installer for the Switchboard application.
- **Takes**: A suite of tools and interfaces designed for recording, reviewing and playing back takes in a virtual production environment.
- **TextureShare**: Share textures and data between processes
- **TimedDataMonitor**: Utilities to monitor inputs that can be time synchronized.

### Web

- **HttpBlueprint**: Allows for sending and receiving HTTP requests in Blueprint

### Xgecontroller

- **XGEController**

---

## 5. Engine Runtime Modules

These are core Unreal Engine runtime modules. You can add them as module dependencies in your mod's `Build.cs` file.

### Core

- `BuildSettings`
- `Core`
- `CoreOnline`
- `CoreUObject`
- `Projects`

### Engine

- `Engine`
- `EngineMessages`
- `EngineSettings`
- `Launch`
- `UnrealGame`

### Rendering

- `D3D12RHI`
- `OpenGLDrv`
- `RHI`
- `RHICore`
- `RenderCore`
- `Renderer`
- `SlateNullRenderer`
- `SlateRHIRenderer`
- `VulkanRHI`

### UI / Slate

- `AdvancedWidgets`
- `AppFramework`
- `Slate`
- `SlateCore`
- `UMG`
- `WidgetCarousel`

### Input

- `InputCore`
- `InputDevice`

### Audio

- `AVEncoder`
- `AudioAnalyzer`
- `AudioCaptureCore`
- `AudioCodecEngine`
- `AudioExtensions`
- `AudioMixer`
- `AudioMixerCore`
- `AudioPlatformConfiguration`
- `NonRealtimeAudioRenderer`
- `SignalProcessing`
- `SoundFieldRendering`

### Animation

- `AnimGraphRuntime`
- `AnimationCore`
- `ClothingSystemRuntimeCommon`
- `ClothingSystemRuntimeInterface`
- `ClothingSystemRuntimeNv`

### Physics

- `PhysicsCore`

### Gameplay

- `AIModule`
- `GameplayDebugger`
- `GameplayTags`
- `GameplayTasks`

### Navigation

- `NavigationSystem`
- `Navmesh`

### Networking

- `Messaging`
- `MessagingCommon`
- `MessagingRpc`
- `NetworkFile`
- `NetworkFileSystem`
- `Networking`
- `Sockets`

### Media

- `GameplayMediaEncoder`
- `ImageCore`
- `ImageWrapper`
- `ImageWriteQueue`
- `Media`
- `MediaAssets`
- `MediaUtils`

### Movie / Sequencer

- `LevelSequence`
- `MoviePlayer`
- `MoviePlayerProxy`
- `MovieScene`
- `MovieSceneCapture`
- `MovieSceneTracks`

### Serialization

- `Cbor`
- `Json`
- `JsonUtilities`
- `Serialization`
- `XmlParser`

### Asset / Registry

- `AssetRegistry`

### Geometry

- `GeometryCore`
- `GeometryFramework`
- `MeshConversion`
- `MeshDescription`
- `RawMesh`
- `SkeletalMeshDescription`
- `StaticMeshDescription`

### Landscape / Foliage

- `Foliage`
- `Landscape`

### World Elements

- `TypedElementFramework`
- `TypedElementRuntime`

### Live Link

- `LiveLinkAnimationCore`
- `LiveLinkInterface`
- `LiveLinkMessageBusFramework`

### Camera

- `CinematicCamera`

### Other Runtime Modules

- `AESBlockEncryptor`
- `AVIWriter`
- `Advertising`
- `Analytics`
- `AnalyticsET`
- `AnalyticsSwrve`
- `AnalyticsVisualEditing`
- `AndroidAdvertising`
- `AndroidLocalNotification`
- `AndroidRuntimeSettings`
- `ApplicationCore`
- `AudioCaptureAndroid`
- `AudioCaptureAudioUnit`
- `AudioCaptureRtAudio`
- `AudioCaptureWasapi`
- `AudioLinkCore`
- `AudioLinkEngine`
- `AudioMixerAndroid`
- `AudioMixerAudioUnit`
- `AudioMixerCoreAudio`
- `AudioMixerPlatformAudioLink`
- `AudioMixerSDL`
- `AudioMixerXAudio2`
- `AutomationMessages`
- `AutomationTest`
- `AutomationWorker`
- `BackgroundHTTP`
- `BackgroundHTTPFileHash`
- `BinkAudioDecoder`
- `BlockEncryptionHandlerComponent`
- `BlowFishBlockEncryptor`
- `BlueprintRuntime`
- `BuildPatchServices`
- `CADKernel`
- `CEF3Utils`
- `CUDA`
- `Chaos`
- `ChaosCore`
- `ChaosSolverEngine`
- `ChaosVDRuntime`
- `ChaosVehiclesCore`
- `ChaosVehiclesEngine`
- `ClientPilot`
- `ColorManagement`
- `Constraints`
- `CookOnTheFly`
- `CrashReportCore`
- `D3D11RHI`
- `DataflowCore`
- `DataflowEngine`
- `DatasmithCore`
- `DeveloperSettings`
- `DirectLink`
- `EncryptionHandlerComponent`
- `EventLoop`
- `EventLoopUnitTests`
- `ExternalRpcRegistry`
- `EyeTracker`
- `FieldNotification`
- `FieldSystemEngine`
- `FriendsAndChat`
- `GameMenuBuilder`
- `GeometryCollectionEngine`
- `HTTP`
- `HardwareSurvey`
- `HeadMountedDisplay`
- `HttpNetworkReplayStreaming`
- `HttpServer`
- `IESFile`
- `IOSAdvertising`
- `IOSAudio`
- `IOSLocalNotification`
- `IOSPlatformFeatures`
- `IOSRuntimeSettings`
- `IPC`
- `Icmp`
- `ImageDownload`
- `InstallBundleManager`
- `InteractiveToolsFramework`
- `InterchangeCore`
- `InterchangeEngine`
- `IoStoreOnDemand`
- `IrisCore`
- `IrisStub`
- `LaunchDaemonMessages`
- `LauncherCheck`
- `LauncherPlatform`
- `LocalFileNetworkReplayStreaming`
- `MaterialShaderQualitySettings`
- `MeshConversionEngineTypes`
- `MeshUtilitiesCommon`
- `MetalRHI`
- `NetCommon`
- `NetCore`
- `NetworkReplayStreaming`
- `NullDrv`
- `OodleDataCompression`
- `Overlay`
- `PacketHandler`
- `PakFile`
- `PerfCounters`
- `PortalMessages`
- `PortalProxies`
- `PortalRpc`
- `PortalServices`
- `PosixShim`
- `PreLoadScreen`
- `PropertyPath`
- `RSA`
- `RSAEncryptionHandlerComponent`
- `RSAKeyAESEncryption`
- `ReliabilityHandlerComponent`
- `RuntimeAssetCache`
- `SSL`
- `SandboxFile`
- `SaveGameNetworkReplayStreaming`
- `SessionMessages`
- `SessionServices`
- `Stomp`
- `StorageServerClient`
- `StreamEncryptionHandlerComponent`
- `StreamingFile`
- `StreamingPauseRendering`
- `SymsLib`
- `SynthBenchmark`
- `TelemetryUtils`
- `TimeManagement`
- `TraceLog`
- `TwoFishBlockEncryptor`
- `UELibrary`
- `UnixCommonStartup`
- `VectorVM`
- `VirtualFileCache`
- `Voice`
- `Voronoi`
- `WebBrowser`
- `WebBrowserTexture`
- `WebSockets`
- `WindowsPlatformFeatures`
- `XMPP`
- `XORBlockEncryptor`
- `XORStreamEncryptor`

---

## 6. How to Use These in Your Mod

### Depending on a Game Module (Build.cs)

```csharp
public class MyMod : ModuleRules
{
    public MyMod(ReadOnlyTargetRules Target) : base(Target)
    {
        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "FactoryGame",        // Main FactoryGame module
            "EnhancedInput",      // Example engine plugin module
        });
    }
}
```

### Depending on an Engine Plugin (.uplugin)

```json
{
    "FileVersion": 3,
    "Version": 1,
    "VersionName": "1.0",
    "FriendlyName": "My Mod",
    "Description": "A custom Satisfactory mod.",
    "Category": "Satisfactory Mods",
    "Modules": [
        {
            "Name": "MyMod",
            "Type": "Runtime",
            "LoadingPhase": "Default"
        }
    ],
    "Plugins": [
        {
            "Name": "AbstractInstance",
            "Enabled": true
        },
        {
            "Name": "EnhancedInput",
            "Enabled": true
        }
    ]
}
```

---

## 7. Custom Mods in This Repository

These mods are developed in this repository and are available as dependencies for your own server-side Satisfactory mods. All four target **Win64 and Linux** dedicated-server platforms only; `RequiredOnRemote` is `false` for every mod so players never need to install them.

---

### SMLWebSocket

*RFC 6455 WebSocket client with SSL/OpenSSL support for Alpakit-packaged mods.*

**Module:** `SMLWebSocket` — `ServerOnly`, LoadingPhase: `Default`  
**SemVersion:** `1.0.0`

#### Why it is needed

Satisfactory uses a custom Coffee Stain Studios Unreal Engine build that omits the engine's `WebSockets` module. `SMLWebSocket` fills that gap with a direct `FSocket` + OpenSSL implementation, enabling any mod to open a `wss://` connection (for example, to the Discord Gateway at `wss://gateway.discord.gg/`).

#### Delegates

| Delegate | Signature | When it fires |
|----------|-----------|---------------|
| `OnConnected` | `()` | Handshake succeeded; connection is ready |
| `OnMessage` | `(FString Message)` | A UTF-8 text frame was received |
| `OnBinaryMessage` | `(TArray<uint8> Data, bool bIsFinal)` | A binary frame (or fragment) was received |
| `OnClosed` | `(int32 StatusCode, FString Reason)` | Connection closed (either side) |
| `OnError` | `(FString ErrorMessage)` | Connection or protocol error |
| `OnReconnecting` | `(int32 AttemptNumber, float DelaySeconds)` | About to retry after a non-user-initiated disconnect |

#### Key properties

| Property | Type | Default | Purpose |
|----------|------|---------|---------|
| `bAutoReconnect` | `bool` | `true` | Reconnect automatically when the server drops the connection |
| `ReconnectInitialDelaySeconds` | `float` | `2.0` | Initial back-off delay before the first retry |
| `MaxReconnectDelaySeconds` | `float` | `30.0` | Cap on the exponential back-off |
| `MaxReconnectAttempts` | `int32` | `0` | Maximum retries (0 = unlimited) |

#### Depending on SMLWebSocket

**`.uplugin`**

```json
{
    "Name": "SMLWebSocket",
    "Enabled": true,
    "SemVersion": "^1.0.0"
}
```

**`Build.cs`**

```csharp
PublicDependencyModuleNames.AddRange(new string[] { "SMLWebSocket" });
```

#### C++ usage example

```cpp
#include "SMLWebSocketClient.h"

// Create in your subsystem's Initialize()
WebSocket = USMLWebSocketClient::CreateWebSocketClient(this);
WebSocket->bAutoReconnect = true;
WebSocket->ReconnectInitialDelaySeconds = 2.0f;
WebSocket->MaxReconnectDelaySeconds     = 30.0f;

// Bind delegates
WebSocket->OnConnected.AddDynamic(this, &UMySubsystem::HandleConnected);
WebSocket->OnMessage.AddDynamic(this,   &UMySubsystem::HandleMessage);
WebSocket->OnClosed.AddDynamic(this,    &UMySubsystem::HandleClosed);
WebSocket->OnError.AddDynamic(this,     &UMySubsystem::HandleError);

// Connect to the Discord Gateway
// BotToken must be loaded from your config (INI) and should never be logged or
// exposed in error messages to avoid leaking credentials.
TMap<FString, FString> Headers;
Headers.Add(TEXT("Authorization"), TEXT("Bot ") + BotToken);
WebSocket->Connect(TEXT("wss://gateway.discord.gg/?v=10&encoding=json"), {}, Headers);

// Send a text message
WebSocket->SendText(TEXT("{\"op\":2,\"d\":{\"token\":\"Bot ...\"}}"));

// Graceful shutdown in Deinitialize()
if (WebSocket)
{
    WebSocket->Close(1000, TEXT("Server shutting down"));
    WebSocket = nullptr;
}
```

#### Dependencies

| Dependency | Version | Purpose |
|------------|---------|---------|
| SML | `^3.11.3` | Module load ordering |

---

### DiscordBridge

*Two-way bridge between Satisfactory's in-game chat and a Discord text channel.*

**Module:** `DiscordBridge` — `ServerOnly`, LoadingPhase: `Default`  
**SemVersion:** `1.0.2`

#### Features

- Two-way real-time chat relay (game ↔ Discord) with configurable format strings (`%PlayerName%`, `%Username%`, `%Message%`, `%ServerName%`)
- Server online/offline status announcements (`ServerOnlineMessage` / `ServerOfflineMessage`)
- Live player-count Discord presence (`PlayerCountPresenceFormat`, configurable update interval)
- Whitelist management via Discord or in-game `!whitelist` commands, with optional Discord role integration
- Optional integration with **BanSystem** — provides the bot connection for ban/unban Discord commands
- Optional integration with **TicketSystem** — provides the bot connection for the button-based ticket panel

#### Required bot intents

Enable these in the [Discord Developer Portal](https://discord.com/developers/applications) under **Bot → Privileged Gateway Intents**:

| Intent | Purpose |
|--------|---------|
| Server Members Intent | Role checks for whitelist and ban commands |
| Message Content Intent | Read messages sent in the bridged channel |

The bot also needs **Send Messages** and **Read Message History** permissions in the target channel, and **Manage Roles** when using `!whitelist role` commands.

#### Configuration

Primary config file (never overwritten by mod updates):

```
<ServerRoot>/FactoryGame/Mods/DiscordBridge/Config/DefaultDiscordBridge.ini
```

Auto-backup (auto-restored if the primary file is missing):

```
<ServerRoot>/FactoryGame/Saved/Config/DiscordBridge.ini
```

| Essential setting | Description |
|-------------------|-------------|
| `BotToken` | Discord bot token |
| `ChannelId` | Snowflake ID of the bridged Discord text channel |
| `ServerName` | Display name shown in outgoing Discord messages |

#### Depending on DiscordBridge

**`.uplugin`** (optional dependency)

```json
{
    "Name": "DiscordBridge",
    "Enabled": true,
    "Optional": true,
    "SemVersion": "^1.0.0"
}
```

**`Build.cs`**

```csharp
PublicDependencyModuleNames.AddRange(new string[] { "DiscordBridge" });
```

Guard all call-sites with `FModuleManager::IsModuleLoaded("DiscordBridge")` when the dependency is optional.

#### Dependencies

| Dependency | Version | Notes |
|------------|---------|-------|
| SML | `^3.11.3` | Required |
| SMLWebSocket | `^1.0.0` | Required — provides the Discord Gateway WebSocket client |
| BanSystem | `^1.0.0` | Optional |
| TicketSystem | `^1.0.0` | Optional |

---

### BanSystem

*Standalone Steam + EOS ban system for Satisfactory dedicated servers.*

**Module:** `BanSystem` — `ServerOnly`, LoadingPhase: `PostDefault`  
**SemVersion:** `1.0.0`

#### Features

- Permanent and timed bans for **Steam 64-bit IDs** and **EOS Product User IDs** independently
- Bans are enforced on join and survive server restarts (JSON storage)
- In-game admin chat commands; also works from the server console
- Blueprint-accessible and C++ linkable public API
- Discord command integration — standalone (own `BotToken`) or paired (shares DiscordBridge's bot)

#### In-game admin commands

| Command | Description |
|---------|-------------|
| `/steamban <Steam64Id\|Name> [min] [reason]` | Permanent or timed Steam ban |
| `/steamunban <Steam64Id>` | Remove a Steam ban |
| `/steambanlist` | List all active Steam bans |
| `/eosban <EOSProductUserId\|Name> [min] [reason]` | Permanent or timed EOS ban |
| `/eosunban <EOSProductUserId>` | Remove an EOS ban |
| `/eosbanlist` | List all active EOS bans |
| `/banbyname <Name> [min] [reason]` | Ban a connected player on all platforms at once |
| `/playerids [Name]` | Show platform IDs of all (or one) connected player(s) |

#### Discord commands

| Command | Description |
|---------|-------------|
| `!steamban` / `!steamunban` / `!steambanlist` | Steam ban management via Discord |
| `!eosban` / `!eosunban` / `!eosbanlist` | EOS ban management via Discord |
| `!banbyname` / `!playerids` | Cross-platform ban / ID lookup via Discord |

All commands are gated by `DiscordCommandRoleId` in `DefaultBanSystem.ini`. The guild owner is always permitted.

#### Discord modes of operation

| Mode | How to activate |
|------|----------------|
| **Standalone** | Set `BotToken` in `DefaultBanSystem.ini`. BanSystem connects to Discord independently — no DiscordBridge required. |
| **Paired** | Leave `BotToken` empty. When DiscordBridge is installed it provides the bot connection automatically with zero extra configuration. |

#### Ban storage

```
<ServerRoot>/FactoryGame/Saved/BanSystem/SteamBans.json
<ServerRoot>/FactoryGame/Saved/BanSystem/EOSBans.json
```

Files are loaded at startup. Expired timed bans are pruned automatically before the first player can connect.

#### ID format reference

| Platform | Format | Example |
|----------|--------|---------|
| Steam | 17-digit decimal starting with `7656119` | `76561198000000000` |
| EOS PUID | 32 lowercase hex characters | `00020aed06f0a6958c3c067fb4b73d51` |

Static validation helpers: `USteamBanSubsystem::IsValidSteam64Id(FString)` and `UEOSBanSubsystem::IsValidEOSProductUserId(FString)`.

#### Depending on BanSystem

**`.uplugin`** (optional dependency)

```json
{
    "Name": "BanSystem",
    "Enabled": true,
    "Optional": true,
    "SemVersion": "^1.0.0"
}
```

**`Build.cs`**

```csharp
PublicDependencyModuleNames.AddRange(new string[] { "BanSystem" });
```

#### C++ API

```cpp
#include "Steam/SteamBanSubsystem.h"
#include "EOS/EOSBanSubsystem.h"
#include "BanIdResolver.h"
#include "BanPlayerLookup.h"

// Resolve both platform IDs from a connecting player
FResolvedBanId Ids = FBanIdResolver::Resolve(PlayerState->GetUniqueNetId());
if (Ids.HasSteamId())   UE_LOG(MyLog, Log, TEXT("Steam64: %s"), *Ids.Steam64Id);
if (Ids.HasEOSPuid())   UE_LOG(MyLog, Log, TEXT("PUID:    %s"), *Ids.EOSProductUserId);

// Ban / unban
USteamBanSubsystem* Steam = GI->GetSubsystem<USteamBanSubsystem>();
Steam->BanPlayer(TEXT("76561198000000000"), TEXT("Cheating"), 0, TEXT("MyMod")); // 0 = permanent

UEOSBanSubsystem* EOS = GI->GetSubsystem<UEOSBanSubsystem>();
EOS->BanPlayer(TEXT("00020aed06f0a6958c3c067fb4b73d51"), TEXT("Spam"), 60, TEXT("MyMod")); // 60 min

// Check ban status
FString Reason;
if (Steam->IsPlayerBanned(TEXT("76561198000000000"), Reason)) { /* ... */ }

// React to ban/unban events
Steam->OnPlayerBanned.AddDynamic(this,   &UMyClass::HandleSteamBanned);
EOS->OnPlayerUnbanned.AddDynamic(this,   &UMyClass::HandleEOSUnbanned);

// Find a connected player by name
FResolvedBanId Ids2; FString FoundName; TArray<FString> Ambiguous;
if (FBanPlayerLookup::FindPlayerByName(World, TEXT("SomePlayer"), Ids2, FoundName, Ambiguous))
{
    // Ids2.Steam64Id / Ids2.EOSProductUserId are populated
}
```

#### Dependencies

| Dependency | Version | Notes |
|------------|---------|-------|
| SML | `^3.11.3` | Required |
| SMLWebSocket | `^1.0.0` | Optional — only needed when `BotToken` is set for standalone Discord mode |

---

### TicketSystem

*Button-based Discord support-ticket panel for Satisfactory dedicated servers.*

**Module:** `TicketSystem` — `ServerOnly`, LoadingPhase: `Default`  
**SemVersion:** `1.0.0`

#### Features

- Members click a button to open a private ticket channel; no slash commands needed
- A reason modal collects context before the channel is created
- Built-in ticket types: **Whitelist Request**, **Help / Support**, **Report a Player**
- Unlimited custom ticket reasons (`TicketReason=Label|Desc` entries in the config)
- Admin/support role @mentioned in every new ticket channel
- **Close Ticket** button deletes the channel when the issue is resolved
- Standalone mode (own `BotToken`) or paired mode (shares DiscordBridge's bot)

#### Modes of operation

| Mode | How to activate |
|------|----------------|
| **Standalone** | Set `BotToken` in `DefaultTickets.ini`. TicketSystem connects to Discord on its own — no DiscordBridge required. |
| **Paired** | Leave `BotToken` empty. When DiscordBridge is also installed it calls `UTicketSubsystem::SetProvider(this)` and powers the panel through its own connection. DiscordBridge always takes priority; any `BotToken` set in `DefaultTickets.ini` is silently ignored when DiscordBridge is present. |

#### Required bot permissions (standalone mode)

| Permission | Purpose |
|------------|---------|
| Manage Channels | Create and delete private ticket channels |
| View Channel | Read the channel list |
| Send Messages | Post the ticket panel, welcome message, and close button |

#### Configuration

```
<ServerRoot>/FactoryGame/Mods/TicketSystem/Config/DefaultTickets.ini
```

| Setting | Default | Description |
|---------|---------|-------------|
| `BotToken` | *(empty)* | Discord bot token for standalone mode |
| `TicketNotifyRoleId` | *(empty)* | Role @mentioned in every new ticket; also authorises `!ticket-panel` |
| `TicketPanelChannelId` | *(empty)* | Channel where the button panel is posted |
| `TicketCategoryId` | *(empty)* | Discord category under which ticket channels are created |
| `TicketChannelId` | *(empty)* | Admin notification channel (comma-separated IDs supported) |
| `TicketWhitelistEnabled` | `True` | Show the Whitelist Request button |
| `TicketHelpEnabled` | `True` | Show the Help / Support button |
| `TicketReportEnabled` | `True` | Show the Report a Player button |
| `TicketReason=Label\|Desc` | *(none)* | Add a custom ticket-reason button (up to 25 total across all buttons). The `|` character is the actual separator in the INI file; the backslash here is markdown-table escaping only. |

#### Depending on TicketSystem

**`.uplugin`** (optional dependency)

```json
{
    "Name": "TicketSystem",
    "Enabled": true,
    "Optional": true,
    "SemVersion": "^1.0.0"
}
```

**`Build.cs`**

```csharp
PublicDependencyModuleNames.AddRange(new string[] { "TicketSystem" });
```

#### Provider architecture (`IDiscordBridgeProvider`)

TicketSystem communicates with Discord exclusively through `IDiscordBridgeProvider` (`Source/TicketSystem/Public/IDiscordBridgeProvider.h`). Any mod that wants to drive the ticket panel must:

1. Add `"TicketSystem"` as a dependency in `.uplugin` and `Build.cs`.
2. Inherit `IDiscordBridgeProvider` in its subsystem class.
3. Implement all pure-virtual methods (send messages, create/delete channels, respond to interactions, show modals, etc.).
4. Call `UTicketSubsystem::SetProvider(this)` in `Initialize()` and `SetProvider(nullptr)` in `Deinitialize()`.

```cpp
void UMyDiscordSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (UTicketSubsystem* Tickets = GetGameInstance()->GetSubsystem<UTicketSubsystem>())
        Tickets->SetProvider(this);
}

void UMyDiscordSubsystem::Deinitialize()
{
    if (UTicketSubsystem* Tickets = GetGameInstance()->GetSubsystem<UTicketSubsystem>())
        Tickets->SetProvider(nullptr);
    Super::Deinitialize();
}
```

#### Dependencies

| Dependency | Version | Notes |
|------------|---------|-------|
| SML | `^3.11.3` | Required |
| SMLWebSocket | `^1.0.0` | Required — provides the Discord Gateway WebSocket client |
| DiscordBridge | `^1.0.2` | Optional — provides paired mode |

---

## 8. Mod Dependency Map

The diagram below shows how the four custom mods relate to each other.

```
SMLWebSocket   (no mod-level dependencies)
     ▲
     │  required by
     ├──────────────────────────────────────────────┐
     │                                              │
DiscordBridge ─────────── optional ─────────► BanSystem
     │                    (provides bot conn)       │
     │  optional                                    │
     │  (provides bot conn + SetProvider)           │
     └──────────────────────────────────────────────► TicketSystem
                         (BanSystem also optional to TicketSystem via shared provider pattern)
```

| Relationship | Direction | Mechanism |
|---|---|---|
| DiscordBridge → SMLWebSocket | Required | Module dependency in `.uplugin` / `Build.cs` |
| BanSystem → SMLWebSocket | Optional | Only active when `BotToken` is set for standalone Discord mode |
| DiscordBridge → BanSystem | Optional | DiscordBridge implements `IBanDiscordCommandProvider` and registers with `UBanDiscordSubsystem` |
| DiscordBridge → TicketSystem | Optional | DiscordBridge calls `UTicketSubsystem::SetProvider(this)` |
| BanSystem standalone | Independent | BanSystem can run without DiscordBridge when its own `BotToken` is configured |
| TicketSystem standalone | Independent | TicketSystem can run without DiscordBridge when its own `BotToken` is configured |

