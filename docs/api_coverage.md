# Copertura dell'API di Roblox in Blocky

Generato da `test_api_coverage` (tools/roblox_api): ogni membro provato davvero nel Luau del motore.
Rifallo con `node tools/roblox_api/gen_probe.mjs` e `build/bin/test_api_coverage.exe`.

| | Ci sono | Totale | Copertura |
|---|---|---|---|
| **Funzionante** (fa davvero la cosa) | 404 | 4083 | **9.9%** |
| **Presente** (non fa crashare lo script; comprende lo strato generato) | 3230 | 4083 | **79.1%** |
| Proprieta' | 1894 | 2347 | 80.7% |
| Metodi | 1038 | 1381 | 75.2% |
| Eventi | 298 | 355 | 83.9% |
| Enum | 642 | 656 | 97.9% |
| Classi che si possono ottenere | 422 | 511 | 82.6% |

Proprieta' che puntano a un oggetto (non verificabili leggendole): 152.

## Classi piu' usate dai giochi

| Classe | Funzionante | Presente | Proprieta' | Metodi | Eventi | Cosa manca |
|---|---|---|---|---|---|---|
| AnimationTrack | 0.0% | 0.0% | 0/8 | 0/12 | 0/4 | _nessun oggetto concreto su cui provarla_ |
| GlobalDataStore | 0.0% | 0.0% | 0/0 | 0/6 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| Path | 0.0% | 0.0% | 0/1 | 0/3 | 0/2 | _nessun oggetto concreto su cui provarla_ |
| ParticleEmitter | 7.7% | 7.7% | 2/37 | 1/2 | 0/0 | Acceleration, Brightness, Color, Drag, EmissionDirection, FlipbookBlendFrames, FlipbookFramerate, FlipbookIncompatible, FlipbookLayout, FlipbookMode, FlipbookSizeX, FlipbookSizeY, FlipbookStartRandom, Lifetime, LightEmission, LightInfluence, LockedToPart, Orientation, RotSpeed, Rotation, Shape, ShapeInOut, ShapePartial, ShapeStyle, Size, Speed, SpreadAngle, Squash, Texture, TextureContent, TimeSca... |
| ClickDetector | 14.3% | 14.3% | 0/3 | 0/0 | 1/4 | CursorIcon, CursorIconContent, MaxActivationDistance, MouseHoverEnter (evento), MouseHoverLeave (evento), RightMouseClick (evento) |
| Sound | 58.6% | 58.6% | 7/18 | 4/4 | 6/7 | AcousticSimulationEnabled, AudioContent, LoopRegion, PlayOnRemove, PlaybackLoudness, PlaybackRegion, PlaybackRegionsEnabled, RollOffMaxDistance, RollOffMinDistance, RollOffMode, TimePosition, DidLoop (evento) |
| SpawnLocation | 0.0% | 80.0% | 4/5 | 0/0 | 0/0 | TeamColor |
| Team | 40.0% | 80.0% | 1/2 | 1/1 | 2/2 | TeamColor |
| Trail | 0.0% | 81.2% | 12/15 | 1/1 | 0/0 | Color, Transparency, WidthScale |
| Beam | 0.0% | 89.5% | 16/18 | 1/1 | 0/0 | Color, Transparency |
| ImageLabel | 25.0% | 91.7% | 11/12 | 0/0 | 0/0 | SliceCenter |
| ImageButton | 18.8% | 93.8% | 15/16 | 0/0 | 0/0 | SliceCenter |
| TextBox | 28.6% | 94.3% | 28/29 | 2/3 | 3/3 | FontFace, IsFocused() |
| DataModel | 22.2% | 94.4% | 10/11 | 2/2 | 5/5 | Genre |
| TextButton | 33.3% | 95.2% | 20/21 | 0/0 | 0/0 | FontFace |
| TextLabel | 33.3% | 95.2% | 20/21 | 0/0 | 0/0 | FontFace |
| BasePart | 50.8% | 95.4% | 36/39 | 24/24 | 2/2 | CurrentPhysicalProperties, CustomPhysicalProperties, ResizeableFaces |
| ProximityPrompt | 29.2% | 95.8% | 13/14 | 2/2 | 8/8 | GamepadKeyCode |
| Player | 10.3% | 96.6% | 25/27 | 24/24 | 7/7 | TeamColor, User |
| Instance | 76.2% | 97.6% | 3/4 | 30/30 | 8/8 | Capabilities |
| Animation | 0.0% | 100.0% | 2/2 | 0/0 | 0/0 |  |
| Animator | 0.0% | 100.0% | 4/4 | 6/6 | 1/1 |  |
| Attachment | 0.0% | 100.0% | 7/7 | 1/1 | 0/0 |  |
| BillboardGui | 0.0% | 100.0% | 14/14 | 0/0 | 0/0 |  |
| BindableEvent | 100.0% | 100.0% | 0/0 | 1/1 | 1/1 |  |
| BindableFunction | 100.0% | 100.0% | 0/0 | 1/1 | 0/0 |  |
| BoolValue | 100.0% | 100.0% | 1/1 | 0/0 | 1/1 |  |
| CFrameValue | 100.0% | 100.0% | 1/1 | 0/0 | 1/1 |  |
| Camera | 30.8% | 100.0% | 16/16 | 9/9 | 1/1 |  |
| CollectionService | 58.3% | 100.0% | 0/0 | 10/10 | 2/2 |  |
| ContextActionService | 37.5% | 100.0% | 0/0 | 14/14 | 2/2 |  |
| DataStoreService | 16.7% | 100.0% | 0/0 | 6/6 | 0/0 |  |
| Debris | 100.0% | 100.0% | 0/0 | 1/1 | 0/0 |  |
| Explosion | 0.0% | 100.0% | 7/7 | 0/0 | 1/1 |  |
| Folder | 0.0% | 100.0% | 2/2 | 0/0 | 0/0 |  |
| Frame | 0.0% | 100.0% | 1/1 | 0/0 | 0/0 |  |
| GuiObject | 21.6% | 100.0% | 21/21 | 0/0 | 16/16 |  |
| Highlight | 0.0% | 100.0% | 6/6 | 0/0 | 0/0 |  |
| HttpService | 80.0% | 100.0% | 1/1 | 9/9 | 0/0 |  |
| Humanoid | 37.1% | 100.0% | 29/29 | 23/23 | 18/18 |  |
| IntValue | 100.0% | 100.0% | 1/1 | 0/0 | 1/1 |  |
| Lighting | 0.0% | 100.0% | 18/18 | 5/5 | 1/1 |  |
| MarketplaceService | 8.6% | 100.0% | 0/0 | 27/27 | 8/8 |  |
| MeshPart | 12.5% | 100.0% | 6/6 | 2/2 | 0/0 |  |
| Model | 46.2% | 100.0% | 4/4 | 9/9 | 0/0 |  |
| NumberValue | 100.0% | 100.0% | 1/1 | 0/0 | 1/1 |  |
| ObjectValue | 100.0% | 100.0% | 0/0 | 0/0 | 1/1 |  |
| PVInstance | 100.0% | 100.0% | 0/0 | 2/2 | 0/0 |  |
| Part | 100.0% | 100.0% | 1/1 | 0/0 | 0/0 |  |
| PathfindingService | 50.0% | 100.0% | 0/0 | 2/2 | 0/0 |  |
| Players | 32.1% | 100.0% | 8/8 | 16/16 | 4/4 |  |
| PointLight | 100.0% | 100.0% | 1/1 | 0/0 | 0/0 |  |
| RemoteEvent | 100.0% | 100.0% | 0/0 | 3/3 | 2/2 |  |
| RemoteFunction | 100.0% | 100.0% | 0/0 | 2/2 | 0/0 |  |
| RunService | 38.5% | 100.0% | 2/2 | 15/15 | 9/9 |  |
| ScreenGui | 40.0% | 100.0% | 5/5 | 0/0 | 0/0 |  |
| Seat | 0.0% | 100.0% | 1/1 | 1/1 | 0/0 |  |
| SoundService | 0.0% | 100.0% | 13/13 | 6/6 | 0/0 |  |
| SpotLight | 66.7% | 100.0% | 3/3 | 0/0 | 0/0 |  |
| StarterGui | 33.3% | 100.0% | 2/2 | 4/4 | 0/0 |  |
| StringValue | 100.0% | 100.0% | 1/1 | 0/0 | 1/1 |  |
| SurfaceGui | 0.0% | 100.0% | 10/10 | 0/0 | 0/0 |  |
| Teams | 100.0% | 100.0% | 0/0 | 1/1 | 0/0 |  |
| TeleportService | 36.4% | 100.0% | 0/0 | 9/9 | 2/2 |  |
| TextChatService | 25.0% | 100.0% | 4/4 | 5/5 | 3/3 |  |
| Tool | 83.3% | 100.0% | 6/6 | 2/2 | 4/4 |  |
| TweenService | 33.3% | 100.0% | 0/0 | 3/3 | 0/0 |  |
| UICorner | 0.0% | 100.0% | 5/5 | 0/0 | 0/0 |  |
| UIListLayout | 20.0% | 100.0% | 5/5 | 0/0 | 0/0 |  |
| UserInputService | 21.2% | 100.0% | 16/16 | 24/24 | 26/26 |  |
| Vector3Value | 100.0% | 100.0% | 1/1 | 0/0 | 1/1 |  |
| VehicleSeat | 0.0% | 100.0% | 10/10 | 1/1 | 0/0 |  |
| WeldConstraint | 0.0% | 100.0% | 2/2 | 0/0 | 0/0 |  |
| Workspace | 12.9% | 100.0% | 23/23 | 7/7 | 1/1 |  |

## Tutte le altre

| Classe | Funzionante | Presente | Proprieta' | Metodi | Eventi | Cosa manca |
|---|---|---|---|---|---|---|
| AnimationImportData | 0.0% | 0.0% | 0/2 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| AppHandle | 0.0% | 0.0% | 0/0 | 0/4 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| AssetPatchSettings | 0.0% | 0.0% | 0/3 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| BackpackItem | 0.0% | 0.0% | 0/2 | 0/0 | 0/0 | TextureContent, TextureId |
| BaseCoreGuiConfiguration | 0.0% | 0.0% | 0/1 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| BaseImportData | 0.0% | 0.0% | 0/3 | 0/0 | 0/2 | _nessun oggetto concreto su cui provarla_ |
| BubbleChatConfiguration | 0.0% | 0.0% | 0/15 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| CanvasGroup | 0.0% | 0.0% | 0/2 | 0/0 | 0/0 | GroupColor3, GroupTransparency |
| Capture | 0.0% | 0.0% | 0/5 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| CapturesViewConfiguration | 0.0% | 0.0% | 0/1 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| ChannelSelectorSoundEffect | 0.0% | 0.0% | 0/1 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| ChannelTabsConfiguration | 0.0% | 0.0% | 0/12 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| ChatInputBarConfiguration | 0.0% | 0.0% | 0/14 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| ChatWindowConfiguration | 0.0% | 0.0% | 0/15 | 0/1 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| ChatWindowMessageProperties | 0.0% | 0.0% | 0/5 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| ConfigSnapshot | 0.0% | 0.0% | 0/2 | 0/3 | 0/1 | _nessun oggetto concreto su cui provarla_ |
| DataStore | 0.0% | 0.0% | 0/0 | 0/4 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| DataStoreInfo | 0.0% | 0.0% | 0/3 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| DataStoreKey | 0.0% | 0.0% | 0/1 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| DataStoreKeyInfo | 0.0% | 0.0% | 0/3 | 0/2 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| DataStoreKeyPages | 0.0% | 0.0% | 0/1 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| DataStoreListingPages | 0.0% | 0.0% | 0/1 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| DataStoreObjectVersionInfo | 0.0% | 0.0% | 0/3 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| DebuggerBreakpoint | 0.0% | 0.0% | 0/6 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| DockWidgetPluginGui | 0.0% | 0.0% | 0/1 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| DynamicRotate | 0.0% | 0.0% | 0/1 | 0/0 | 0/0 | BaseAngle |
| EditableImage | 0.0% | 0.0% | 0/1 | 0/10 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| EditableMesh | 0.0% | 0.0% | 0/1 | 0/93 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| ExecutedRemoteCommand | 0.0% | 0.0% | 0/0 | 0/3 | 0/1 | _nessun oggetto concreto su cui provarla_ |
| Feature | 0.0% | 0.0% | 0/4 | 0/0 | 0/0 | FaceId, InOut, LeftRight, TopBottom |
| FeatureMatchingSession | 0.0% | 0.0% | 0/0 | 0/11 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| FeatureSearchParams | 0.0% | 0.0% | 0/10 | 0/8 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| Fire | 0.0% | 0.0% | 0/6 | 0/0 | 0/0 | Color, Enabled, Heat, SecondaryColor, Size, TimeScale |
| GlobalSettings | 0.0% | 0.0% | 0/0 | 0/2 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| GroupImportData | 0.0% | 0.0% | 0/3 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| ImportSession | 0.0% | 0.0% | 0/0 | 0/0 | 0/2 | _nessun oggetto concreto su cui provarla_ |
| InputObject | 0.0% | 0.0% | 0/5 | 0/1 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| KeyframeSequenceProvider | 0.0% | 0.0% | 0/0 | 0/4 | 0/0 | RegisterActiveKeyframeSequence(), RegisterKeyframeSequence(), GetAnimationsAsync(), GetKeyframeSequenceAsync() |
| Logger | 0.0% | 0.0% | 0/2 | 0/6 | 0/1 | _nessun oggetto concreto su cui provarla_ |
| MLSession | 0.0% | 0.0% | 0/0 | 0/1 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| MaterialImportData | 0.0% | 0.0% | 0/11 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| MemoryStoreDistributedCounter | 0.0% | 0.0% | 0/0 | 0/2 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| MemoryStoreHashMap | 0.0% | 0.0% | 0/0 | 0/5 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| MemoryStoreQueue | 0.0% | 0.0% | 0/0 | 0/4 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| MemoryStoreSortedMap | 0.0% | 0.0% | 0/0 | 0/6 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| MeshImportData | 0.0% | 0.0% | 0/21 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| Mouse | 0.0% | 0.0% | 0/10 | 0/0 | 0/8 | _nessun oggetto concreto su cui provarla_ |
| NetworkMarker | 0.0% | 0.0% | 0/0 | 0/0 | 0/1 | _nessun oggetto concreto su cui provarla_ |
| NetworkReplicator | 0.0% | 0.0% | 0/0 | 0/1 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| OrderedDataStore | 0.0% | 0.0% | 0/0 | 0/1 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| PackageLink | 0.0% | 0.0% | 0/5 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| Pages | 0.0% | 0.0% | 0/1 | 0/2 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| ParticipantHandle | 0.0% | 0.0% | 0/0 | 0/6 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| PatchMapping | 0.0% | 0.0% | 0/3 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| PlayerGui | 0.0% | 0.0% | 0/2 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| PlayerListConfiguration | 0.0% | 0.0% | 0/1 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| PlayerScripts | 0.0% | 0.0% | 0/0 | 0/8 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| Plugin | 0.0% | 0.0% | 0/2 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| PluginDragEvent | 0.0% | 0.0% | 0/4 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| PluginGui | 0.0% | 0.0% | 0/1 | 0/1 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| PluginMenu | 0.0% | 0.0% | 0/2 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| PluginToolbarButton | 0.0% | 0.0% | 0/3 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| RootImportData | 0.0% | 0.0% | 0/26 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| ScreenshotHud | 0.0% | 0.0% | 0/8 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| ScriptDebugger | 0.0% | 0.0% | 0/3 | 0/12 | 0/6 | _nessun oggetto concreto su cui provarla_ |
| SelfViewConfiguration | 0.0% | 0.0% | 0/1 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| Smoke | 0.0% | 0.0% | 0/6 | 0/0 | 0/0 | Color, Enabled, Opacity, RiseVelocity, Size, TimeScale |
| Sparkles | 0.0% | 0.0% | 0/3 | 0/0 | 0/0 | Enabled, SparkleColor, TimeScale |
| StandardQueue | 0.0% | 0.0% | 0/0 | 0/3 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| Studio | 0.0% | 0.0% | 0/1 | 0/0 | 0/0 | LocalAssetsFolder |
| SyncScriptBuilder | 0.0% | 0.0% | 0/4 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| TeleportAsyncResult | 0.0% | 0.0% | 0/2 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| Terrain | 0.0% | 0.0% | 0/8 | 0/29 | 0/0 | _non esiste in Blocky_ |
| TerrainIterateOperation | 0.0% | 0.0% | 0/0 | 0/1 | 0/1 | _nessun oggetto concreto su cui provarla_ |
| TerrainModifyOperation | 0.0% | 0.0% | 0/0 | 0/1 | 0/1 | _nessun oggetto concreto su cui provarla_ |
| TerrainReadOperation | 0.0% | 0.0% | 0/0 | 0/0 | 0/1 | _nessun oggetto concreto su cui provarla_ |
| TerrainWriteOperation | 0.0% | 0.0% | 0/0 | 0/2 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| TestCase | 0.0% | 0.0% | 0/4 | 0/8 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| TestExecutionContext | 0.0% | 0.0% | 0/0 | 0/1 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| TestFailure | 0.0% | 0.0% | 0/4 | 0/3 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| TestResult | 0.0% | 0.0% | 0/9 | 0/2 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| TestRunResult | 0.0% | 0.0% | 0/7 | 0/1 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| TestSuite | 0.0% | 0.0% | 0/2 | 0/2 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| TextChatMessage | 0.0% | 0.0% | 0/7 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| TextFilterResult | 0.0% | 0.0% | 0/0 | 0/2 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| TextFilterTranslatedResult | 0.0% | 0.0% | 0/1 | 0/2 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| TextSource | 0.0% | 0.0% | 0/2 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| TrackerLodController | 0.0% | 0.0% | 0/4 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| Translator | 0.0% | 0.0% | 0/1 | 0/2 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| Tween | 0.0% | 0.0% | 0/1 | 0/0 | 0/0 | TweenInfo |
| UserSettings | 0.0% | 0.0% | 0/0 | 0/2 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| VideoCapture | 0.0% | 0.0% | 0/1 | 0/0 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| VideoSampler | 0.0% | 0.0% | 0/2 | 0/1 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| VirtualInput | 0.0% | 0.0% | 0/0 | 0/6 | 0/0 | _nessun oggetto concreto su cui provarla_ |
| WebSocketClient | 0.0% | 0.0% | 0/1 | 0/2 | 0/3 | _nessun oggetto concreto su cui provarla_ |
| WebStreamClient | 0.0% | 0.0% | 0/1 | 0/2 | 0/4 | _nessun oggetto concreto su cui provarla_ |
| AvatarEditorService | 6.9% | 6.9% | 0/0 | 2/22 | 0/7 | GetAccessoryType(), PromptAllowInventoryReadAccess(), PromptCreateOutfit(), PromptDeleteOutfit(), PromptRenameOutfit(), PromptSaveAvatar(), PromptSetFavorite(), PromptUpdateOutfit(), CheckApplyDefaultClothingAsync(), ConformToAvatarRulesAsync(), GetAvatarRulesAsync(), GetBatchItemDetailsAsync(), GetBundlesByAssetIdAsync(), GetFavoriteAsync(), GetHeadShapesAsync(), GetInventoryAsync(), GetOutfitDet... |
| SocialService | 10.5% | 10.5% | 0/0 | 2/15 | 0/4 | GetPlayersByPartyId(), HideSelfView(), PromptPhoneBook(), ShowSelfView(), CanSendCallInviteAsync(), GetEventRsvpStatusAsync(), GetExperienceEventAsync(), GetPartyAsync(), GetUpcomingExperienceEventsAsync(), PromptFeedbackSubmissionAsync(), PromptLinkSharingAsync(), PromptRateExperienceAsync(), PromptRsvpToEventAsync(), CallInviteStateChanged (evento), GameInvitePromptClosed (evento), PhoneBookProm... |
| VRService | 11.1% | 11.1% | 1/8 | 1/6 | 0/4 | AutomaticScaling, AvatarGestures, ControllerModels, FadeOutViewOnCollision, GuiInputUserCFrame, LaserPointer, ThirdPersonFollowCamEnabled, GetTouchpadMode(), GetUserCFrame(), RecenterUserHeadCFrame(), RequestNavigation(), SetTouchpadMode(), NavigationRequested (evento), TouchpadModeChanged (evento), UserCFrameChanged (evento), UserCFrameEnabled (evento) |
| Stats | 12.9% | 12.9% | 3/22 | 1/9 | 0/0 | ContactsCount, FrameTime, HeartbeatTime, MemoryTrackingEnabled, MovingPrimitivesCount, PhysicsReceiveKbps, PhysicsSendKbps, PhysicsStepTime, PrimitivesCount, RenderCPUFrameTime, RenderGPUFrameTime, SceneDrawcallCount, SceneTriangleCount, ShadowsDrawcallCount, ShadowsTriangleCount, UI2DDrawcallCount, UI2DTriangleCount, UI3DDrawcallCount, UI3DTriangleCount, GetHarmonyQualityLevel(), GetMemoryCategor... |
| ViewportFrame | 40.0% | 40.0% | 2/5 | 0/0 | 0/0 | Ambient, LightColor, LightDirection |
| VoiceChatService | 40.0% | 40.0% | 0/3 | 2/2 | 0/0 | DefaultDistanceAttenuation, EnableDefaultVoice, UseAudioApi |
| BodyColors | 0.0% | 50.0% | 6/12 | 0/0 | 0/0 | HeadColor, LeftArmColor, LeftLegColor, RightArmColor, RightLegColor, TorsoColor |
| RayValue | 50.0% | 50.0% | 0/1 | 0/0 | 1/1 | Value |
| TextChannelWindow | 0.0% | 50.0% | 1/2 | 0/0 | 0/0 | FontFace |
| InputBinding | 0.0% | 59.1% | 12/21 | 1/1 | 0/0 | Backward, Down, Forward, KeyCode, Left, PrimaryModifier, Right, SecondaryModifier, Up |
| Constraint | 0.0% | 75.0% | 3/4 | 0/0 | 0/0 | Color |
| MemoryStoreService | 75.0% | 75.0% | 0/0 | 3/4 | 0/0 | GetDistributedCounter() |
| UIGradient | 0.0% | 75.0% | 6/8 | 0/0 | 0/0 | Color, Transparency |
| GetTextBoundsParams | 0.0% | 80.0% | 4/5 | 0/0 | 0/0 | Font |
| ArcHandles | 0.0% | 83.3% | 0/1 | 0/0 | 5/5 | Axes |
| BubbleChatMessageProperties | 0.0% | 83.3% | 5/6 | 0/0 | 0/0 | FontFace |
| Handles | 0.0% | 85.7% | 1/2 | 0/0 | 5/5 | Faces |
| MaterialVariant | 0.0% | 85.7% | 6/7 | 0/0 | 0/0 | CustomPhysicalProperties |
| ParticleEmitter2D | 0.0% | 87.9% | 27/31 | 2/2 | 0/0 | Color, Size, Squash, Transparency |
| InputActionLabel | 0.0% | 90.9% | 10/11 | 0/0 | 0/0 | FontFace |
| DragDetector | 0.0% | 94.1% | 24/26 | 5/5 | 3/3 | GamepadModeSwitchKeyCode, VRSwitchKeyCode |
| GuiService | 12.5% | 95.8% | 6/7 | 15/15 | 2/2 | TopbarInset |
| Accessory | 0.0% | 100.0% | 1/1 | 0/0 | 0/0 |  |
| AccessoryDescription | 0.0% | 100.0% | 7/7 | 1/1 | 0/0 |  |
| Accoutrement | 0.0% | 100.0% | 1/1 | 0/0 | 0/0 |  |
| Actor | 0.0% | 100.0% | 0/0 | 3/3 | 0/0 |  |
| AdGui | 0.0% | 100.0% | 5/5 | 0/0 | 0/0 |  |
| AdPlacement | 0.0% | 100.0% | 6/6 | 0/0 | 0/0 |  |
| AdPortal | 0.0% | 100.0% | 1/1 | 0/0 | 0/0 |  |
| AdService | 0.0% | 100.0% | 0/0 | 8/8 | 0/0 |  |
| AirController | 0.0% | 100.0% | 7/7 | 0/0 | 0/0 |  |
| AlignOrientation | 0.0% | 100.0% | 12/12 | 0/0 | 0/0 |  |
| AlignPosition | 0.0% | 100.0% | 11/11 | 0/0 | 0/0 |  |
| AnalyticsService | 0.0% | 100.0% | 0/0 | 11/11 | 0/0 |  |
| AngularVelocity | 0.0% | 100.0% | 4/4 | 0/0 | 0/0 |  |
| AnimationClip | 0.0% | 100.0% | 3/3 | 0/0 | 0/0 |  |
| AnimationClipProvider | 0.0% | 100.0% | 0/0 | 5/5 | 0/0 |  |
| AnimationConstraint | 0.0% | 100.0% | 8/8 | 0/0 | 0/0 |  |
| AnimationNodeDefinition | 0.0% | 100.0% | 2/2 | 4/4 | 1/1 |  |
| AnimationRigData | 0.0% | 100.0% | 0/0 | 1/1 | 0/0 |  |
| AnimationValueNodeDefinition | 0.0% | 100.0% | 2/2 | 0/0 | 0/0 |  |
| AssetDeliveryProxy | 0.0% | 100.0% | 3/3 | 0/0 | 0/0 |  |
| AssetService | 0.0% | 100.0% | 0/0 | 24/24 | 0/0 |  |
| Atmosphere | 0.0% | 100.0% | 6/6 | 0/0 | 0/0 |  |
| AtmosphereSensor | 0.0% | 100.0% | 2/2 | 0/0 | 0/0 |  |
| AudioAnalyzer | 0.0% | 100.0% | 4/4 | 4/4 | 1/1 |  |
| AudioChannelMixer | 0.0% | 100.0% | 1/1 | 3/3 | 1/1 |  |
| AudioChannelSplitter | 0.0% | 100.0% | 1/1 | 3/3 | 1/1 |  |
| AudioChorus | 0.0% | 100.0% | 4/4 | 3/3 | 1/1 |  |
| AudioCompressor | 0.0% | 100.0% | 6/6 | 3/3 | 1/1 |  |
| AudioDeviceInput | 0.0% | 100.0% | 4/4 | 5/5 | 1/1 |  |
| AudioDeviceOutput | 0.0% | 100.0% | 0/0 | 3/3 | 1/1 |  |
| AudioDistortion | 0.0% | 100.0% | 2/2 | 3/3 | 1/1 |  |
| AudioEcho | 0.0% | 100.0% | 6/6 | 3/3 | 1/1 |  |
| AudioEmitter | 0.0% | 100.0% | 8/8 | 9/9 | 1/1 |  |
| AudioEqualizer | 0.0% | 100.0% | 5/5 | 3/3 | 1/1 |  |
| AudioFader | 0.0% | 100.0% | 2/2 | 3/3 | 1/1 |  |
| AudioFilter | 0.0% | 100.0% | 5/5 | 4/4 | 1/1 |  |
| AudioFlanger | 0.0% | 100.0% | 4/4 | 3/3 | 1/1 |  |
| AudioGate | 0.0% | 100.0% | 4/4 | 3/3 | 1/1 |  |
| AudioLimiter | 0.0% | 100.0% | 3/3 | 3/3 | 1/1 |  |
| AudioListener | 0.0% | 100.0% | 6/6 | 9/9 | 1/1 |  |
| AudioPitchShifter | 0.0% | 100.0% | 3/3 | 3/3 | 1/1 |  |
| AudioPlayer | 0.0% | 100.0% | 13/13 | 7/7 | 3/3 |  |
| AudioRecorder | 0.0% | 100.0% | 2/2 | 9/9 | 1/1 |  |
| AudioReverb | 0.0% | 100.0% | 13/13 | 3/3 | 1/1 |  |
| AudioSearchParams | 0.0% | 100.0% | 10/10 | 0/0 | 0/0 |  |
| AudioSpeechToText | 0.0% | 100.0% | 3/3 | 3/3 | 1/1 |  |
| AudioStreamReader | 0.0% | 100.0% | 4/4 | 5/5 | 1/1 |  |
| AudioStreamWriter | 0.0% | 100.0% | 4/4 | 5/5 | 1/1 |  |
| AudioTextToSpeech | 0.0% | 100.0% | 12/12 | 8/8 | 3/3 |  |
| AudioTremolo | 0.0% | 100.0% | 7/7 | 3/3 | 1/1 |  |
| AudioWindSynthesizer | 0.0% | 100.0% | 4/4 | 3/3 | 1/1 |  |
| AvatarAccessoryRules | 0.0% | 100.0% | 15/15 | 0/0 | 0/0 |  |
| AvatarAnimationRules | 0.0% | 100.0% | 11/11 | 0/0 | 0/0 |  |
| AvatarBodyRules | 0.0% | 100.0% | 15/15 | 0/0 | 0/0 |  |
| AvatarClothingRules | 0.0% | 100.0% | 14/14 | 0/0 | 0/0 |  |
| AvatarCollisionRules | 0.0% | 100.0% | 5/5 | 0/0 | 0/0 |  |
| AvatarCreationService | 0.0% | 100.0% | 0/0 | 16/16 | 3/3 |  |
| AvatarEffectDescription | 0.0% | 100.0% | 2/2 | 0/0 | 0/0 |  |
| BadgeService | 20.0% | 100.0% | 0/0 | 5/5 | 0/0 |  |
| BallSocketConstraint | 0.0% | 100.0% | 8/8 | 0/0 | 0/0 |  |
| BasePlayerGui | 0.0% | 100.0% | 0/0 | 1/1 | 0/0 |  |
| BaseScript | 0.0% | 100.0% | 3/3 | 0/0 | 0/0 |  |
| BaseWrap | 0.0% | 100.0% | 6/6 | 0/0 | 0/0 |  |
| BinaryStringValue | 100.0% | 100.0% | 0/0 | 0/0 | 1/1 |  |
| BloomEffect | 0.0% | 100.0% | 3/3 | 0/0 | 0/0 |  |
| BlurEffect | 0.0% | 100.0% | 1/1 | 0/0 | 0/0 |  |
| BodyPartDescription | 0.0% | 100.0% | 4/4 | 0/0 | 0/0 |  |
| Bone | 0.0% | 100.0% | 2/2 | 0/0 | 0/0 |  |
| BoxHandleAdornment | 0.0% | 100.0% | 2/2 | 0/0 | 0/0 |  |
| BrickColorValue | 100.0% | 100.0% | 1/1 | 0/0 | 1/1 |  |
| BuoyancySensor | 0.0% | 100.0% | 2/2 | 0/0 | 0/0 |  |
| CaptureService | 0.0% | 100.0% | 0/0 | 14/14 | 3/3 |  |
| CharacterMesh | 0.0% | 100.0% | 7/7 | 0/0 | 0/0 |  |
| Chat | 45.5% | 100.0% | 2/2 | 8/8 | 1/1 |  |
| ChorusSoundEffect | 0.0% | 100.0% | 3/3 | 0/0 | 0/0 |  |
| ClimbController | 0.0% | 100.0% | 4/4 | 0/0 | 0/0 |  |
| Clothing | 0.0% | 100.0% | 1/1 | 0/0 | 0/0 |  |
| Clouds | 0.0% | 100.0% | 4/4 | 0/0 | 0/0 |  |
| Color3Value | 100.0% | 100.0% | 1/1 | 0/0 | 1/1 |  |
| ColorCorrectionEffect | 0.0% | 100.0% | 4/4 | 0/0 | 0/0 |  |
| ColorGradingEffect | 0.0% | 100.0% | 1/1 | 0/0 | 0/0 |  |
| CommerceService | 0.0% | 100.0% | 0/0 | 4/4 | 1/1 |  |
| CompositeValueCurve | 0.0% | 100.0% | 1/1 | 2/2 | 0/0 |  |
| CompressorSoundEffect | 0.0% | 100.0% | 5/5 | 0/0 | 0/0 |  |
| ConeHandleAdornment | 0.0% | 100.0% | 4/4 | 0/0 | 0/0 |  |
| ConfigService | 0.0% | 100.0% | 0/0 | 4/4 | 0/0 |  |
| ContentProvider | 7.7% | 100.0% | 2/2 | 10/10 | 1/1 |  |
| ControlState | 0.0% | 100.0% | 0/0 | 14/14 | 1/1 |  |
| Controller | 0.0% | 100.0% | 0/0 | 3/3 | 1/1 |  |
| ControllerBase | 0.0% | 100.0% | 3/3 | 0/0 | 0/0 |  |
| ControllerManager | 0.0% | 100.0% | 5/5 | 0/0 | 0/0 |  |
| ControllerPartSensor | 0.0% | 100.0% | 7/7 | 0/0 | 0/0 |  |
| CoreGui | 0.0% | 100.0% | 1/1 | 0/0 | 0/0 |  |
| CoreGuiConfiguration | 0.0% | 0.0% | 0/0 | 0/0 | 0/0 |  |
| CustomLog | 0.0% | 100.0% | 0/0 | 4/4 | 0/0 |  |
| CylinderHandleAdornment | 0.0% | 100.0% | 5/5 | 0/0 | 0/0 |  |
| CylindricalConstraint | 0.0% | 100.0% | 16/16 | 0/0 | 0/0 |  |
| DataModelMesh | 0.0% | 100.0% | 3/3 | 0/0 | 0/0 |  |
| DataStoreGetOptions | 0.0% | 100.0% | 1/1 | 0/0 | 0/0 |  |
| DataStoreIncrementOptions | 0.0% | 100.0% | 0/0 | 2/2 | 0/0 |  |
| DataStoreOptions | 0.0% | 100.0% | 1/1 | 1/1 | 0/0 |  |
| DataStoreSetOptions | 0.0% | 100.0% | 0/0 | 2/2 | 0/0 |  |
| DebuggerManager | 0.0% | 100.0% | 1/1 | 3/3 | 2/2 |  |
| DebuggerWatch | 0.0% | 100.0% | 1/1 | 0/0 | 0/0 |  |
| Decal | 0.0% | 100.0% | 18/18 | 0/0 | 0/0 |  |
| DepthOfFieldEffect | 0.0% | 100.0% | 4/4 | 0/0 | 0/0 |  |
| Dialog | 0.0% | 100.0% | 10/10 | 1/1 | 1/1 |  |
| DialogChoice | 0.0% | 100.0% | 4/4 | 0/0 | 0/0 |  |
| DigitsRigDescription | 0.0% | 100.0% | 26/26 | 5/5 | 0/0 |  |
| DistortionSoundEffect | 0.0% | 100.0% | 1/1 | 0/0 | 0/0 |  |
| Dragger | 0.0% | 100.0% | 0/0 | 4/4 | 0/0 |  |
| DraggerService | 0.0% | 100.0% | 15/15 | 0/0 | 0/0 |  |
| EchoSoundEffect | 0.0% | 100.0% | 4/4 | 0/0 | 0/0 |  |
| EncodingService | 0.0% | 100.0% | 0/0 | 7/7 | 0/0 |  |
| EqualizerSoundEffect | 0.0% | 100.0% | 3/3 | 0/0 | 0/0 |  |
| EulerRotationCurve | 0.0% | 100.0% | 1/1 | 5/5 | 0/0 |  |
| ExperienceInviteOptions | 0.0% | 100.0% | 4/4 | 0/0 | 0/0 |  |
| ExperienceNotificationService | 0.0% | 100.0% | 0/0 | 2/2 | 1/1 |  |
| ExplorerServiceVisibilityService | 0.0% | 100.0% | 0/0 | 1/1 | 0/0 |  |
| FaceControls | 0.0% | 100.0% | 50/50 | 0/0 | 0/0 |  |
| FaceInstance | 0.0% | 100.0% | 1/1 | 0/0 | 0/0 |  |
| FeatureMatchingService | 0.0% | 100.0% | 0/0 | 4/4 | 0/0 |  |
| FeatureMatchingTable | 0.0% | 100.0% | 1/1 | 1/1 | 0/0 |  |
| FileMesh | 0.0% | 100.0% | 4/4 | 0/0 | 0/0 |  |
| FlangeSoundEffect | 0.0% | 100.0% | 3/3 | 0/0 | 0/0 |  |
| FloatCurve | 0.0% | 100.0% | 1/1 | 7/7 | 0/0 |  |
| FluidForceSensor | 0.0% | 100.0% | 5/5 | 1/1 | 0/0 |  |
| FollowRotationBoneBehavior | 0.0% | 100.0% | 2/2 | 0/0 | 0/0 |  |
| FollowTwistBoneBehavior | 0.0% | 100.0% | 4/4 | 0/0 | 0/0 |  |
| ForceField | 0.0% | 100.0% | 1/1 | 0/0 | 0/0 |  |
| GamepadService | 100.0% | 100.0% | 1/1 | 2/2 | 0/0 |  |
| GeneratedFolder | 0.0% | 100.0% | 0/0 | 1/1 | 0/0 |  |
| GenerationService | 0.0% | 100.0% | 0/0 | 11/11 | 0/0 |  |
| GeometryService | 41.7% | 100.0% | 0/0 | 12/12 | 0/0 |  |
| GroundController | 0.0% | 100.0% | 11/11 | 0/0 | 0/0 |  |
| GroupService | 16.7% | 100.0% | 0/0 | 6/6 | 0/0 |  |
| GuiBase2d | 20.0% | 100.0% | 9/9 | 0/0 | 1/1 |  |
| GuiBase3d | 0.0% | 100.0% | 3/3 | 0/0 | 0/0 |  |
| GuiButton | 25.0% | 100.0% | 4/4 | 0/0 | 8/8 |  |
| HandleAdornment | 0.0% | 100.0% | 5/5 | 0/0 | 4/4 |  |
| HapticEffect | 0.0% | 100.0% | 4/4 | 4/4 | 1/1 |  |
| HapticService | 100.0% | 100.0% | 0/0 | 4/4 | 0/0 |  |
| HingeConstraint | 0.0% | 100.0% | 14/14 | 0/0 | 0/0 |  |
| HttpRbxApiService | 0.0% | 100.0% | 0/0 | 1/1 | 0/0 |  |
| HumanoidDescription | 0.0% | 100.0% | 40/40 | 8/8 | 2/2 |  |
| HumanoidRigDescription | 0.0% | 100.0% | 89/89 | 8/8 | 0/0 |  |
| IKControl | 0.0% | 100.0% | 7/7 | 6/6 | 0/0 |  |
| ImageHandleAdornment | 0.0% | 100.0% | 3/3 | 0/0 | 0/0 |  |
| IncrementalPatchBuilder | 0.0% | 100.0% | 6/6 | 0/0 | 0/0 |  |
| InputAction | 0.0% | 100.0% | 3/3 | 1/1 | 3/3 |  |
| InputContext | 0.0% | 100.0% | 3/3 | 0/0 | 0/0 |  |
| InsertService | 16.7% | 100.0% | 0/0 | 6/6 | 0/0 |  |
| InstanceAdornment | 0.0% | 0.0% | 0/0 | 0/0 | 0/0 |  |
| JointInstance | 0.0% | 100.0% | 4/4 | 0/0 | 0/0 |  |
| Keyframe | 0.0% | 100.0% | 1/1 | 6/6 | 0/0 |  |
| KeyframeMarker | 0.0% | 100.0% | 1/1 | 0/0 | 0/0 |  |
| KeyframeSequence | 0.0% | 100.0% | 0/0 | 3/3 | 0/0 |  |
| LanguageService | 0.0% | 100.0% | 0/0 | 1/1 | 0/0 |  |
| LayerCollector | 20.0% | 100.0% | 4/4 | 1/1 | 0/0 |  |
| Light | 50.0% | 100.0% | 4/4 | 0/0 | 0/0 |  |
| LineForce | 0.0% | 100.0% | 5/5 | 0/0 | 0/0 |  |
| LineHandleAdornment | 0.0% | 100.0% | 2/2 | 0/0 | 0/0 |  |
| LinearVelocity | 0.0% | 100.0% | 14/14 | 0/0 | 0/0 |  |
| LiveSyncService | 0.0% | 100.0% | 1/1 | 1/1 | 1/1 |  |
| LocalizationService | 14.3% | 100.0% | 2/2 | 5/5 | 0/0 |  |
| LocalizationTable | 0.0% | 100.0% | 1/1 | 11/11 | 0/0 |  |
| LogService | 0.0% | 100.0% | 0/0 | 9/9 | 1/1 |  |
| MLService | 0.0% | 100.0% | 0/0 | 4/4 | 0/0 |  |
| MakeupDescription | 0.0% | 100.0% | 3/3 | 1/1 | 0/0 |  |
| MarkerCurve | 0.0% | 100.0% | 1/1 | 4/4 | 0/0 |  |
| MatchmakingService | 0.0% | 100.0% | 0/0 | 3/3 | 0/0 |  |
| MaterialService | 0.0% | 100.0% | 0/0 | 3/3 | 0/0 |  |
| MessagingService | 100.0% | 100.0% | 0/0 | 2/2 | 0/0 |  |
| MicroProfilerService | 0.0% | 100.0% | 0/0 | 3/3 | 1/1 |  |
| ModerationService | 0.0% | 100.0% | 0/0 | 6/6 | 0/0 |  |
| ModuleScript | 100.0% | 100.0% | 1/1 | 0/0 | 0/0 |  |
| Motor | 0.0% | 100.0% | 3/3 | 1/1 | 0/0 |  |
| NetworkClient | 0.0% | 100.0% | 0/0 | 0/0 | 2/2 |  |
| NetworkServer | 0.0% | 100.0% | 0/0 | 1/1 | 0/0 |  |
| NetworkSettings | 0.0% | 100.0% | 9/9 | 0/0 | 0/0 |  |
| NoCollisionConstraint | 0.0% | 100.0% | 1/1 | 0/0 | 0/0 |  |
| NotificationService | 0.0% | 100.0% | 0/0 | 0/0 | 2/2 |  |
| NumberPose | 0.0% | 100.0% | 1/1 | 0/0 | 0/0 |  |
| Object | 100.0% | 100.0% | 1/1 | 2/2 | 1/1 |  |
| PVAdornment | 0.0% | 0.0% | 0/0 | 0/0 | 0/0 |  |
| Pants | 0.0% | 100.0% | 2/2 | 0/0 | 0/0 |  |
| PartAdornment | 0.0% | 0.0% | 0/0 | 0/0 | 0/0 |  |
| PartOperation | 0.0% | 100.0% | 4/4 | 1/1 | 0/0 |  |
| Path2D | 0.0% | 100.0% | 5/5 | 13/13 | 1/1 |  |
| PathfindingLink | 0.0% | 100.0% | 2/2 | 0/0 | 0/0 |  |
| PathfindingModifier | 0.0% | 100.0% | 2/2 | 0/0 | 0/0 |  |
| PhysicsActivitySensor | 0.0% | 100.0% | 3/3 | 0/0 | 0/0 |  |
| PhysicsService | 100.0% | 100.0% | 0/0 | 8/8 | 0/0 |  |
| PitchShiftSoundEffect | 0.0% | 100.0% | 1/1 | 0/0 | 0/0 |  |
| PlaceSettings | 0.0% | 100.0% | 0/0 | 3/3 | 0/0 |  |
| PlayerViewService | 0.0% | 100.0% | 0/0 | 1/1 | 0/0 |  |
| PluginAction | 0.0% | 100.0% | 4/4 | 0/0 | 0/0 |  |
| PolicyService | 50.0% | 100.0% | 0/0 | 2/2 | 0/0 |  |
| Pose | 0.0% | 100.0% | 1/1 | 3/3 | 0/0 |  |
| PoseBase | 0.0% | 100.0% | 3/3 | 0/0 | 0/0 |  |
| PostEffect | 0.0% | 100.0% | 1/1 | 0/0 | 0/0 |  |
| ProceduralModel | 0.0% | 100.0% | 2/2 | 2/2 | 0/0 |  |
| ProximityPromptService | 0.0% | 100.0% | 3/3 | 0/0 | 8/8 |  |
| PyramidHandleAdornment | 0.0% | 100.0% | 4/4 | 0/0 | 0/0 |  |
| QueueService | 0.0% | 100.0% | 0/0 | 1/1 | 0/0 |  |
| RTAnimationTracker | 0.0% | 100.0% | 0/0 | 0/0 | 2/2 |  |
| RealtimeMedia | 0.0% | 100.0% | 3/3 | 6/6 | 4/4 |  |
| RecommendationService | 0.0% | 100.0% | 0/0 | 8/8 | 0/0 |  |
| ReflectionMetadataClass | 0.0% | 100.0% | 4/4 | 0/0 | 0/0 |  |
| ReflectionMetadataItem | 0.0% | 100.0% | 16/16 | 0/0 | 0/0 |  |
| ReflectionService | 0.0% | 100.0% | 0/0 | 5/5 | 0/0 |  |
| RemoteCommandService | 0.0% | 100.0% | 0/0 | 4/4 | 0/0 |  |
| RenderingTest | 0.0% | 100.0% | 13/13 | 1/1 | 1/1 |  |
| ReplicatedFirst | 100.0% | 100.0% | 0/0 | 1/1 | 0/0 |  |
| ReverbSoundEffect | 0.0% | 100.0% | 5/5 | 0/0 | 0/0 |  |
| RodConstraint | 0.0% | 100.0% | 6/6 | 0/0 | 0/0 |  |
| RomarkService | 0.0% | 100.0% | 0/0 | 1/1 | 0/0 |  |
| RopeConstraint | 0.0% | 100.0% | 9/9 | 0/0 | 0/0 |  |
| RotationCurve | 0.0% | 100.0% | 1/1 | 7/7 | 0/0 |  |
| RuntimeContentService | 0.0% | 100.0% | 0/0 | 0/0 | 3/3 |  |
| SceneAnalysisService | 0.0% | 100.0% | 0/0 | 7/7 | 0/0 |  |
| Script | 100.0% | 100.0% | 1/1 | 0/0 | 0/0 |  |
| ScriptContext | 0.0% | 100.0% | 0/0 | 2/2 | 1/1 |  |
| ScriptService | 0.0% | 100.0% | 0/0 | 1/1 | 0/0 |  |
| ScrollingFrame | 0.0% | 100.0% | 20/20 | 2/2 | 0/0 |  |
| Selection | 0.0% | 100.0% | 1/1 | 0/0 | 1/1 |  |
| SelectionBox | 0.0% | 100.0% | 3/3 | 0/0 | 0/0 |  |
| SelectionLasso | 0.0% | 0.0% | 0/0 | 0/0 | 0/0 |  |
| SelectionSphere | 0.0% | 100.0% | 2/2 | 0/0 | 0/0 |  |
| SensorBase | 0.0% | 100.0% | 1/1 | 0/0 | 1/1 |  |
| SerializationService | 0.0% | 100.0% | 0/0 | 4/4 | 0/0 |  |
| ServiceProvider | 40.0% | 100.0% | 0/0 | 2/2 | 3/3 |  |
| SharedTableRegistry | 0.0% | 100.0% | 0/0 | 2/2 | 0/0 |  |
| Shirt | 0.0% | 100.0% | 2/2 | 0/0 | 0/0 |  |
| ShirtGraphic | 0.0% | 100.0% | 3/3 | 0/0 | 0/0 |  |
| SkateboardController | 0.0% | 100.0% | 2/2 | 0/0 | 1/1 |  |
| Sky | 0.0% | 100.0% | 21/21 | 0/0 | 0/0 |  |
| SlidingBallConstraint | 0.0% | 100.0% | 14/14 | 0/0 | 0/0 |  |
| SoundEffect | 0.0% | 100.0% | 2/2 | 0/0 | 0/0 |  |
| SoundGroup | 0.0% | 100.0% | 1/1 | 0/0 | 0/0 |  |
| SpecialMesh | 0.0% | 100.0% | 1/1 | 0/0 | 0/0 |  |
| SphereHandleAdornment | 0.0% | 100.0% | 2/2 | 0/0 | 0/0 |  |
| SpringConstraint | 0.0% | 100.0% | 11/11 | 0/0 | 0/0 |  |
| StarterGear | 0.0% | 100.0% | 1/1 | 0/0 | 0/0 |  |
| StarterPlayer | 0.0% | 100.0% | 23/23 | 0/0 | 0/0 |  |
| StateMachineTransitionDefinition | 0.0% | 100.0% | 2/2 | 0/0 | 0/0 |  |
| StudioAttachment | 0.0% | 100.0% | 5/5 | 0/0 | 0/0 |  |
| StudioService | 0.0% | 100.0% | 6/6 | 0/0 | 0/0 |  |
| StudioTestService | 0.0% | 100.0% | 0/0 | 5/5 | 0/0 |  |
| StyleBase | 0.0% | 100.0% | 0/0 | 3/3 | 1/1 |  |
| StyleDerive | 0.0% | 100.0% | 1/1 | 0/0 | 0/0 |  |
| StyleLink | 0.0% | 0.0% | 0/0 | 0/0 | 0/0 |  |
| StyleQuery | 0.0% | 100.0% | 1/1 | 4/4 | 0/0 |  |
| StyleRule | 0.0% | 100.0% | 3/3 | 9/9 | 0/0 |  |
| StyleSheet | 0.0% | 100.0% | 0/0 | 2/2 | 0/0 |  |
| SunRaysEffect | 0.0% | 100.0% | 2/2 | 0/0 | 0/0 |  |
| SurfaceAppearance | 0.0% | 100.0% | 11/11 | 0/0 | 0/0 |  |
| SurfaceGuiBase | 0.0% | 100.0% | 2/2 | 0/0 | 0/0 |  |
| SurfaceLight | 100.0% | 100.0% | 3/3 | 0/0 | 0/0 |  |
| SurfaceSelection | 0.0% | 100.0% | 1/1 | 0/0 | 0/0 |  |
| SwimController | 0.0% | 100.0% | 5/5 | 0/0 | 0/0 |  |
| TaskScheduler | 0.0% | 100.0% | 4/4 | 0/0 | 0/0 |  |
| TeleportOptions | 0.0% | 100.0% | 3/3 | 2/2 | 0/0 |  |
| TerrainDetail | 0.0% | 100.0% | 5/5 | 0/0 | 0/0 |  |
| TerrainRegion | 0.0% | 100.0% | 1/1 | 0/0 | 0/0 |  |
| TestCoordinatorService | 0.0% | 100.0% | 0/0 | 6/6 | 0/0 |  |
| TestService | 0.0% | 100.0% | 13/13 | 33/33 | 3/3 |  |
| TextChannel | 0.0% | 100.0% | 1/1 | 4/4 | 1/1 |  |
| TextChatCommand | 0.0% | 100.0% | 4/4 | 0/0 | 1/1 |  |
| TextChatMessageProperties | 0.0% | 100.0% | 3/3 | 0/0 | 0/0 |  |
| TextDocument | 0.0% | 100.0% | 1/1 | 1/1 | 0/0 |  |
| TextGenerator | 0.0% | 100.0% | 4/4 | 1/1 | 0/0 |  |
| TextService | 33.3% | 100.0% | 0/0 | 6/6 | 0/0 |  |
| Texture | 0.0% | 100.0% | 4/4 | 0/0 | 0/0 |  |
| Torque | 0.0% | 100.0% | 2/2 | 0/0 | 0/0 |  |
| TorsionSpringConstraint | 0.0% | 100.0% | 9/9 | 0/0 | 0/0 |  |
| TremoloSoundEffect | 0.0% | 100.0% | 3/3 | 0/0 | 0/0 |  |
| TriangleMeshPart | 0.0% | 100.0% | 4/4 | 0/0 | 0/0 |  |
| TrussPart | 0.0% | 100.0% | 1/1 | 0/0 | 0/0 |  |
| TweenBase | 0.0% | 100.0% | 1/1 | 3/3 | 1/1 |  |
| UIAspectRatioConstraint | 0.0% | 100.0% | 3/3 | 0/0 | 0/0 |  |
| UIBlur | 0.0% | 100.0% | 4/4 | 0/0 | 0/0 |  |
| UIDragDetector | 0.0% | 100.0% | 21/21 | 4/4 | 3/3 |  |
| UIFlexItem | 0.0% | 100.0% | 4/4 | 0/0 | 0/0 |  |
| UIGridLayout | 0.0% | 100.0% | 6/6 | 0/0 | 0/0 |  |
| UIGridStyleLayout | 20.0% | 100.0% | 5/5 | 0/0 | 0/0 |  |
| UIPadding | 0.0% | 100.0% | 4/4 | 0/0 | 0/0 |  |
| UIPageLayout | 0.0% | 100.0% | 9/9 | 4/4 | 3/3 |  |
| UIScale | 0.0% | 100.0% | 1/1 | 0/0 | 0/0 |  |
| UIShadow | 0.0% | 100.0% | 10/10 | 0/0 | 0/0 |  |
| UISizeConstraint | 0.0% | 100.0% | 2/2 | 0/0 | 0/0 |  |
| UIStroke | 0.0% | 100.0% | 10/10 | 0/0 | 0/0 |  |
| UITableLayout | 0.0% | 100.0% | 4/4 | 0/0 | 0/0 |  |
| UITextSizeConstraint | 0.0% | 100.0% | 2/2 | 0/0 | 0/0 |  |
| UniqueIdLookupService | 0.0% | 100.0% | 0/0 | 1/1 | 0/0 |  |
| UniversalConstraint | 0.0% | 100.0% | 4/4 | 0/0 | 0/0 |  |
| UnreliableRemoteEvent | 100.0% | 100.0% | 0/0 | 3/3 | 2/2 |  |
| UserGameSettings | 0.0% | 100.0% | 13/13 | 7/7 | 2/2 |  |
| UserService | 0.0% | 100.0% | 0/0 | 2/2 | 0/0 |  |
| ValueCurve | 0.0% | 100.0% | 2/2 | 8/8 | 0/0 |  |
| Vector3Curve | 0.0% | 100.0% | 0/0 | 4/4 | 0/0 |  |
| VectorForce | 0.0% | 100.0% | 3/3 | 0/0 | 0/0 |  |
| VelocityMotor | 0.0% | 100.0% | 3/3 | 0/0 | 0/0 |  |
| VideoCaptureService | 0.0% | 100.0% | 0/0 | 2/2 | 0/0 |  |
| VideoDeviceInput | 0.0% | 100.0% | 4/4 | 0/0 | 0/0 |  |
| VideoDisplay | 0.0% | 100.0% | 7/7 | 3/3 | 1/1 |  |
| VideoFrame | 0.0% | 100.0% | 12/12 | 2/2 | 6/6 |  |
| VideoPlayer | 0.0% | 100.0% | 9/9 | 7/7 | 4/4 |  |
| VideoService | 0.0% | 100.0% | 0/0 | 1/1 | 0/0 |  |
| VoiceChatInternal | 0.0% | 100.0% | 0/0 | 1/1 | 0/0 |  |
| WebSocketService | 0.0% | 100.0% | 0/0 | 1/1 | 0/0 |  |
| Wire | 0.0% | 100.0% | 3/3 | 0/0 | 0/0 |  |
| WireframeHandleAdornment | 0.0% | 100.0% | 2/2 | 5/5 | 0/0 |  |
| WorldModel | 0.0% | 100.0% | 1/1 | 0/0 | 0/0 |  |
| WorldRoot | 94.1% | 100.0% | 0/0 | 17/17 | 0/0 |  |
| WrapDeformer | 0.0% | 100.0% | 0/0 | 3/3 | 0/0 |  |
| WrapLayer | 0.0% | 100.0% | 11/11 | 0/0 | 0/0 |  |
| WrapTarget | 0.0% | 100.0% | 0/0 | 2/2 | 0/0 |  |
| WrapTextureTransfer | 0.0% | 100.0% | 3/3 | 1/1 | 0/0 |  |

## Enum mancanti

AdEventType, AudioCaptureMode, AvatarSettingsAnimationClipsMode, AvatarSettingsAppearanceMode, AvatarSettingsCustomAccessoryMode, AvatarSettingsCustomBodyType, AvatarSettingsCustomClothingMode, AvatarSettingsHitAndTouchDetectionMode, AvatarSettingsLegacyCollisionMode, AvatarUnificationMode, IntentReplicability, StateObjectFieldType, StreamingPauseMode, UITheme
