# Lista di controllo delle API Roblox per Blocky

Generata da `tools/roblox_api/gen_checklist.mjs` dal dump dell'API Roblox
(`tools/roblox_api_dump`, versione 0.747.0.7471481).
Una casella per ogni proprietà, metodo, evento e callback che un gioco può usare,
classe per classe, nei pacchetti di `docs/PROMPT_API_LUAU.md`. Sono esclusi i membri
deprecati, nascosti o riservati a Roblox e ai plugin.

**Regola:** si spunta `[x]` solo quando il membro **funziona come su Roblox** e c'è un test
in `tests/compat` che lo prova. "Esiste e non va in errore" non basta: per quello
c'è già lo strato "presente" (`docs/api_coverage.md`).
| Pacchetto | Classi | Membri |
|---|---|---|
| P2 - L'albero delle Instance | 22 | 86 |
| P3 - Mondo e part | 25 | 237 |
| P4 - Fisica e movimento | 37 | 223 |
| P5 - Personaggi, NPC e animazioni | 28 | 336 |
| P6 - Strumenti e combattimento | 14 | 123 |
| P7 - GUI | 43 | 389 |
| P8 - Input, camera, tempo | 8 | 151 |
| P9 - Effetti, luci, atmosfera, suono | 42 | 291 |
| P10 - Dati, rete e servizi | 38 | 244 |
| **Totale** | | **2080** |

Ogni classe ha anche i membri delle classi da cui eredita (scritte accanto al nome).


## P2 - L'albero delle Instance

### Instance (eredita da Object) _NotCreatable_
- [ ] `Instance.AncestryChanged` (evento: child: Instance, parent: Instance)
- [ ] `Instance.Archivable`: bool
- [ ] `Instance.AttributeChanged` (evento: attribute: string)
- [ ] `Instance.Capabilities`: SecurityCapabilities
- [ ] `Instance.ChildAdded` (evento: child: Instance)
- [ ] `Instance.ChildRemoved` (evento: child: Instance)
- [ ] `Instance.DescendantAdded` (evento: descendant: Instance)
- [ ] `Instance.DescendantRemoving` (evento: descendant: Instance)
- [ ] `Instance.Destroying` (evento: nessun argomento)
- [ ] `Instance.Name`: string
- [ ] `Instance.Parent`: Instance
- [ ] `Instance.Sandboxed`: bool
- [ ] `Instance.StyledPropertiesChanged` (evento: nessun argomento)
- [ ] `Instance:AddTag(tag: string)` -> null
- [ ] `Instance:ClearAllChildren()` -> null
- [ ] `Instance:Clone()` -> Instance
- [ ] `Instance:Destroy()` -> null
- [ ] `Instance:FindFirstAncestor(name: string)` -> Instance
- [ ] `Instance:FindFirstAncestorOfClass(className: string)` -> Instance
- [ ] `Instance:FindFirstAncestorWhichIsA(className: string)` -> Instance
- [ ] `Instance:FindFirstChild(name: string, recursive: bool = false)` -> Instance
- [ ] `Instance:FindFirstChildOfClass(className: string)` -> Instance
- [ ] `Instance:FindFirstChildWhichIsA(className: string, recursive: bool = false)` -> Instance
- [ ] `Instance:FindFirstDescendant(name: string)` -> Instance
- [ ] `Instance:GetActor()` -> Actor
- [ ] `Instance:GetAttribute(attribute: string)` -> Variant
- [ ] `Instance:GetAttributeChangedSignal(attribute: string)` -> RBXScriptSignal
- [ ] `Instance:GetAttributes()` -> Dictionary  _(CustomLuaState)_
- [ ] `Instance:GetChildren()` -> Instances
- [ ] `Instance:GetDescendants()` -> Instances  _(CustomLuaState)_
- [ ] `Instance:GetFullName()` -> string
- [ ] `Instance:GetStyled(name: string, selector: string?)` -> Variant
- [ ] `Instance:GetStyledPropertyChangedSignal(property: string)` -> RBXScriptSignal
- [ ] `Instance:GetTags()` -> Array
- [ ] `Instance:HasTag(tag: string)` -> bool
- [ ] `Instance:IsAncestorOf(descendant: Instance)` -> bool
- [ ] `Instance:IsDescendantOf(ancestor: Instance)` -> bool
- [ ] `Instance:IsPropertyModified(property: string)` -> bool
- [ ] `Instance:QueryDescendants(selector: string)` -> Instances  _(CustomLuaState)_
- [ ] `Instance:RemoveTag(tag: string)` -> null
- [ ] `Instance:ResetPropertyToDefault(property: string)` -> null
- [ ] `Instance:SetAttribute(attribute: string, value: Variant)` -> null
- [ ] `Instance:WaitForChild(childName: string, timeOut: double)` -> Instance  _(CustomLuaState)_

### Folder (eredita da Instance)
- [ ] `Folder.ExplorerIconContent`: Content
- [ ] `Folder.IconTint`: Color3 = 0, 0, 0

### Configuration (eredita da Instance)
_(nessun membro proprio: vedi la classe da cui eredita)_

### ValueBase (eredita da Instance) _NotCreatable_
_(nessun membro proprio: vedi la classe da cui eredita)_

### IntValue (eredita da ValueBase)
- [ ] `IntValue.Changed` (evento: value: int64)
- [ ] `IntValue.Value`: int64 = 0

### NumberValue (eredita da ValueBase)
- [ ] `NumberValue.Changed` (evento: value: double)
- [ ] `NumberValue.Value`: double = 0

### StringValue (eredita da ValueBase)
- [ ] `StringValue.Changed` (evento: value: string)
- [ ] `StringValue.Value`: string = 

### BoolValue (eredita da ValueBase)
- [ ] `BoolValue.Changed` (evento: value: bool)
- [ ] `BoolValue.Value`: bool = false

### ObjectValue (eredita da ValueBase)
- [ ] `ObjectValue.Changed` (evento: value: Instance)
- [ ] `ObjectValue.Value`: Instance

### Vector3Value (eredita da ValueBase)
- [ ] `Vector3Value.Changed` (evento: value: Vector3)
- [ ] `Vector3Value.Value`: Vector3 = 0, 0, 0

### CFrameValue (eredita da ValueBase)
- [ ] `CFrameValue.Changed` (evento: value: CFrame)
- [ ] `CFrameValue.Value`: CFrame = 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1

### Color3Value (eredita da ValueBase)
- [ ] `Color3Value.Changed` (evento: value: Color3)
- [ ] `Color3Value.Value`: Color3 = 0, 0, 0

### BrickColorValue (eredita da ValueBase)
- [ ] `BrickColorValue.Changed` (evento: value: BrickColor)
- [ ] `BrickColorValue.Value`: BrickColor

### RayValue (eredita da ValueBase)
- [ ] `RayValue.Changed` (evento: value: Ray)
- [ ] `RayValue.Value`: Ray = {0, 0, 0}, {0, 0, 0}

### BindableEvent (eredita da Instance)
- [ ] `BindableEvent.Event` (evento: arguments: Tuple)
- [ ] `BindableEvent:Fire(arguments: Tuple)` -> null

### BindableFunction (eredita da Instance)
- [ ] `BindableFunction.OnInvoke` (callback: arguments: Tuple) -> Tuple
- [ ] `BindableFunction:Invoke(arguments: Tuple)` -> Tuple  _(Yields)_

### ModuleScript (eredita da LuaSourceContainer)
- [ ] `ModuleScript.Source`: ProtectedString = 

### LuaSourceContainer (eredita da Instance) _NotCreatable_
_(nessun membro proprio: vedi la classe da cui eredita)_

### BaseScript (eredita da LuaSourceContainer) _NotCreatable_
- [ ] `BaseScript.Disabled`: bool
- [ ] `BaseScript.Enabled`: bool
- [ ] `BaseScript.RunContext`: RunContext

### Script (eredita da BaseScript)
- [ ] `Script.Source`: ProtectedString = 

### LocalScript (eredita da Script)
_(nessun membro proprio: vedi la classe da cui eredita)_

### CollectionService (eredita da Instance) _NotCreatable, Service_
- [ ] `CollectionService.TagAdded` (evento: tag: string)
- [ ] `CollectionService.TagRemoved` (evento: tag: string)
- [ ] `CollectionService:AddTag(instance: Instance, tag: string)` -> null  _(CustomLuaState)_
- [ ] `CollectionService:CreateCollection(query: string, root: Instance = nil)` -> CollectionHandle
- [ ] `CollectionService:GetAllTags()` -> Array
- [ ] `CollectionService:GetCollections(instance: Instance)` -> Array
- [ ] `CollectionService:GetInstanceAddedSignal(tag: string)` -> RBXScriptSignal
- [ ] `CollectionService:GetInstanceRemovedSignal(tag: string)` -> RBXScriptSignal
- [ ] `CollectionService:GetTagged(tag: string)` -> Instances
- [ ] `CollectionService:GetTags(instance: Instance)` -> Array  _(CustomLuaState)_
- [ ] `CollectionService:HasTag(instance: Instance, tag: string)` -> bool  _(CustomLuaState)_
- [ ] `CollectionService:RemoveTag(instance: Instance, tag: string)` -> null  _(CustomLuaState)_

## P3 - Mondo e part

### PVInstance (eredita da Instance) _NotCreatable_
- [ ] `PVInstance:GetPivot()` -> CFrame
- [ ] `PVInstance:PivotTo(targetCFrame: CFrame)` -> null

### BasePart (eredita da PVInstance) _NotCreatable_
- [ ] `BasePart.Anchored`: bool
- [ ] `BasePart.AssemblyAngularVelocity`: Vector3
- [ ] `BasePart.AssemblyCenterOfMass`: Vector3  _(ReadOnly)_
- [ ] `BasePart.AssemblyLinearVelocity`: Vector3
- [ ] `BasePart.AssemblyMass`: float  _(ReadOnly)_
- [ ] `BasePart.AssemblyRootPart`: BasePart  _(ReadOnly)_
- [ ] `BasePart.AudioCanCollide`: bool
- [ ] `BasePart.BackSurface`: SurfaceType
- [ ] `BasePart.BottomSurface`: SurfaceType
- [ ] `BasePart.BrickColor`: BrickColor
- [ ] `BasePart.CFrame`: CFrame
- [ ] `BasePart.CanCollide`: bool
- [ ] `BasePart.CanQuery`: bool
- [ ] `BasePart.CanTouch`: bool
- [ ] `BasePart.CastShadow`: bool
- [ ] `BasePart.CenterOfMass`: Vector3  _(ReadOnly)_
- [ ] `BasePart.CollisionGroup`: string
- [ ] `BasePart.Color`: Color3
- [ ] `BasePart.CurrentPhysicalProperties`: PhysicalProperties  _(ReadOnly)_
- [ ] `BasePart.CustomPhysicalProperties`: PhysicalProperties
- [ ] `BasePart.EnableFluidForces`: bool
- [ ] `BasePart.ExtentsCFrame`: CFrame  _(ReadOnly)_
- [ ] `BasePart.ExtentsSize`: Vector3  _(ReadOnly)_
- [ ] `BasePart.FrontSurface`: SurfaceType
- [ ] `BasePart.LeftSurface`: SurfaceType
- [ ] `BasePart.Locked`: bool
- [ ] `BasePart.Mass`: float  _(ReadOnly)_
- [ ] `BasePart.Massless`: bool
- [ ] `BasePart.MaterialVariant`: string
- [ ] `BasePart.Material`: Material
- [ ] `BasePart.PivotOffset`: CFrame
- [ ] `BasePart.Reflectance`: float
- [ ] `BasePart.ResizeIncrement`: int  _(ReadOnly)_
- [ ] `BasePart.ResizeableFaces`: Faces  _(ReadOnly)_
- [ ] `BasePart.RightSurface`: SurfaceType
- [ ] `BasePart.RootPriority`: int
- [ ] `BasePart.Rotation`: Vector3
- [ ] `BasePart.Size`: Vector3
- [ ] `BasePart.TopSurface`: SurfaceType
- [ ] `BasePart.TouchEnded` (evento: otherPart: BasePart)
- [ ] `BasePart.Touched` (evento: otherPart: BasePart)
- [ ] `BasePart.Transparency`: float
- [ ] `BasePart:AngularAccelerationToTorque(angAcceleration: Vector3, angVelocity: Vector3 = 0, 0, 0)` -> Vector3
- [ ] `BasePart:ApplyAngularImpulse(impulse: Vector3)` -> null
- [ ] `BasePart:ApplyImpulse(impulse: Vector3)` -> null
- [ ] `BasePart:ApplyImpulseAtPosition(impulse: Vector3, position: Vector3)` -> null
- [ ] `BasePart:BindToCollisionSummaries(callback: Function)` -> RBXScriptConnection
- [ ] `BasePart:CanCollideWith(part: BasePart)` -> bool
- [ ] `BasePart:CanSetNetworkOwnership()` -> Tuple
- [ ] `BasePart:GetClosestPointOnSurface(position: Vector3)` -> Vector3
- [ ] `BasePart:GetConnectedParts(recursive: bool = false)` -> Instances
- [ ] `BasePart:GetJoints()` -> Instances
- [ ] `BasePart:GetMass()` -> float
- [ ] `BasePart:GetNetworkOwner()` -> Instance
- [ ] `BasePart:GetNetworkOwnershipAuto()` -> bool
- [ ] `BasePart:GetNoCollisionConstraints()` -> Instances
- [ ] `BasePart:GetTouchingParts()` -> Instances
- [ ] `BasePart:GetVelocityAtPosition(position: Vector3)` -> Vector3
- [ ] `BasePart:IntersectAsync(parts: Instances, collisionfidelity: CollisionFidelity = Default, renderFidelity: RenderFidelity = Automatic)` -> Instance  _(Yields)_
- [ ] `BasePart:IsGrounded()` -> bool
- [ ] `BasePart:Resize(normalId: NormalId, deltaAmount: int)` -> bool
- [ ] `BasePart:SetNetworkOwner(playerInstance: Player = nil)` -> null
- [ ] `BasePart:SetNetworkOwnershipAuto()` -> null
- [ ] `BasePart:SubtractAsync(parts: Instances, collisionfidelity: CollisionFidelity = Default, renderFidelity: RenderFidelity = Automatic)` -> Instance  _(Yields)_
- [ ] `BasePart:TorqueToAngularAcceleration(torque: Vector3, angVelocity: Vector3 = 0, 0, 0)` -> Vector3
- [ ] `BasePart:UnionAsync(parts: Instances, collisionfidelity: CollisionFidelity = Default, renderFidelity: RenderFidelity = Automatic)` -> Instance  _(Yields)_

### FormFactorPart (eredita da BasePart) _NotCreatable_
_(nessun membro proprio: vedi la classe da cui eredita)_

### Part (eredita da FormFactorPart)
- [ ] `Part.Shape`: PartType = Block

### WedgePart (eredita da FormFactorPart)
_(nessun membro proprio: vedi la classe da cui eredita)_

### CornerWedgePart (eredita da BasePart)
_(nessun membro proprio: vedi la classe da cui eredita)_

### TrussPart (eredita da BasePart)
- [ ] `TrussPart.Style`: Style = AlternatingSupports

### MeshPart (eredita da TriangleMeshPart)
- [ ] `MeshPart.DoubleSided`: bool = false
- [ ] `MeshPart.MeshContent`: Content
- [ ] `MeshPart.MeshId`: ContentId = 
- [ ] `MeshPart.RenderFidelity`: RenderFidelity = Automatic
- [ ] `MeshPart.TextureContent`: Content
- [ ] `MeshPart.TextureID`: ContentId = 
- [ ] `MeshPart:ApplyMesh(meshPart: Instance)` -> null
- [ ] `MeshPart:GetSurfaceSlotsAsync()` -> Array  _(Yields)_

### PartOperation (eredita da TriangleMeshPart)
- [ ] `PartOperation.RenderFidelity`: RenderFidelity = Automatic
- [ ] `PartOperation.SmoothingAngle`: float = 0
- [ ] `PartOperation.TriangleCount`: int = 0
- [ ] `PartOperation.UsePartColor`: bool = false
- [ ] `PartOperation:SubstituteGeometry(source: Instance)` -> null

### UnionOperation (eredita da PartOperation)
_(nessun membro proprio: vedi la classe da cui eredita)_

### IntersectOperation (eredita da PartOperation)
_(nessun membro proprio: vedi la classe da cui eredita)_

### NegateOperation (eredita da PartOperation)
_(nessun membro proprio: vedi la classe da cui eredita)_

### SpawnLocation (eredita da Part)
- [ ] `SpawnLocation.AllowTeamChangeOnTouch`: bool = false
- [ ] `SpawnLocation.Duration`: int = 10
- [ ] `SpawnLocation.Enabled`: bool = true
- [ ] `SpawnLocation.Neutral`: bool = true
- [ ] `SpawnLocation.TeamColor`: BrickColor

### Model (eredita da PVInstance)
- [ ] `Model.ExplorerIconContent`: Content
- [ ] `Model.LevelOfDetail`: ModelLevelOfDetail = Automatic
- [ ] `Model.ModelStreamingMode`: ModelStreamingMode = Default
- [ ] `Model.PrimaryPart`: BasePart
- [ ] `Model.WorldPivot`: CFrame = 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1
- [ ] `Model:AddPersistentPlayer(playerInstance: Player = nil)` -> null
- [ ] `Model:GetBoundingBox()` -> undefined
- [ ] `Model:GetExtentsSize()` -> Vector3
- [ ] `Model:GetPersistentPlayers()` -> Instances
- [ ] `Model:GetScale()` -> float
- [ ] `Model:MoveTo(position: Vector3)` -> null
- [ ] `Model:RemovePersistentPlayer(playerInstance: Player = nil)` -> null
- [ ] `Model:ScaleTo(newScaleFactor: float)` -> null
- [ ] `Model:TranslateBy(delta: Vector3)` -> null

### WorldRoot (eredita da Model) _NotCreatable_
- [ ] `WorldRoot:ArePartsTouchingOthers(partList: Instances, overlapIgnored: float = 0.000199999995)` -> bool
- [ ] `WorldRoot:Blockcast(cframe: CFrame, size: Vector3, direction: Vector3, params: RaycastParams = RaycastParams{IgnoreWater=false, Margin=0, SizeGradient=0, BruteForceAllSlow=false, RespectCanCollide=false, CollisionGroup=Default, FilterDescendantsInstances={}})` -> RaycastResult?
- [ ] `WorldRoot:BulkMoveTo(partList: Instances, cframeList: Array, eventMode: BulkMoveMode = FireAllEvents)` -> null
- [ ] `WorldRoot:CollisionGroupSetCollidable(name1: string, name2: string, collidable: bool)` -> null
- [ ] `WorldRoot:CollisionGroupsAreCollidable(name1: string, name2: string)` -> bool
- [ ] `WorldRoot:GetMaxCollisionGroups()` -> int
- [ ] `WorldRoot:GetPartBoundsInBox(cframe: CFrame, size: Vector3, overlapParams: OverlapParams = OverlapParams{MaxParts=0, Tolerance=0, BruteForceAllSlow=false, RespectCanCollide=false, CollisionGroup=Default, FilterDescendantsInstances={}})` -> Instances  _(CustomLuaState)_
- [ ] `WorldRoot:GetPartBoundsInRadius(position: Vector3, radius: float, overlapParams: OverlapParams = OverlapParams{MaxParts=0, Tolerance=0, BruteForceAllSlow=false, RespectCanCollide=false, CollisionGroup=Default, FilterDescendantsInstances={}})` -> Instances  _(CustomLuaState)_
- [ ] `WorldRoot:GetPartsInPart(part: BasePart, overlapParams: OverlapParams = OverlapParams{MaxParts=0, Tolerance=0, BruteForceAllSlow=false, RespectCanCollide=false, CollisionGroup=Default, FilterDescendantsInstances={}})` -> Instances  _(CustomLuaState)_
- [ ] `WorldRoot:GetRegisteredCollisionGroups()` -> Array
- [ ] `WorldRoot:IsCollisionGroupRegistered(name: string)` -> bool
- [ ] `WorldRoot:Raycast(origin: Vector3, direction: Vector3, raycastParams: RaycastParams = RaycastParams{IgnoreWater=false, Margin=0, SizeGradient=0, BruteForceAllSlow=false, RespectCanCollide=false, CollisionGroup=Default, FilterDescendantsInstances={}})` -> RaycastResult?
- [ ] `WorldRoot:RegisterCollisionGroup(name: string)` -> null
- [ ] `WorldRoot:RenameCollisionGroup(from: string, to: string)` -> null
- [ ] `WorldRoot:Shapecast(part: BasePart, direction: Vector3, params: RaycastParams = RaycastParams{IgnoreWater=false, Margin=0, SizeGradient=0, BruteForceAllSlow=false, RespectCanCollide=false, CollisionGroup=Default, FilterDescendantsInstances={}}, cframeOverride: CoordinateFrame? = nil, sizeOverride: Vector3? = nil)` -> RaycastResult?
- [ ] `WorldRoot:Spherecast(position: Vector3, radius: float, direction: Vector3, params: RaycastParams = RaycastParams{IgnoreWater=false, Margin=0, SizeGradient=0, BruteForceAllSlow=false, RespectCanCollide=false, CollisionGroup=Default, FilterDescendantsInstances={}})` -> RaycastResult?
- [ ] `WorldRoot:UnregisterCollisionGroup(name: string)` -> null

### Workspace (eredita da WorldRoot) _NotCreatable, Service_
- [ ] `Workspace.AirDensity`: float
- [ ] `Workspace.AirTurbulenceIntensity`: float
- [ ] `Workspace.AllowThirdPartySales`: bool
- [ ] `Workspace.AuthorityMode`: AuthorityMode
- [ ] `Workspace.ClientAnimatorThrottling`: ClientAnimatorThrottlingMode
- [ ] `Workspace.CurrentCamera`: Camera
- [ ] `Workspace.DistributedGameTime`: double
- [ ] `Workspace.FallHeightEnabled`: bool
- [ ] `Workspace.FallenPartsDestroyHeight`: float
- [ ] `Workspace.GlobalWind`: Vector3
- [ ] `Workspace.Gravity`: float
- [ ] `Workspace.InsertPoint`: Vector3
- [ ] `Workspace.LuauTypeCheckMode`: LuauTypeCheckMode
- [ ] `Workspace.NextGenerationPhysicsSolver`: RolloutState
- [ ] `Workspace.PersistentLoaded` (evento: player: Player)
- [ ] `Workspace.PredictiveStreamingMode`: PredictiveStreamingMode
- [ ] `Workspace.ReplicationFrequency`: ReplicationMode
- [ ] `Workspace.ReplicationThroughput`: ReplicationMode
- [ ] `Workspace.Retargeting`: AnimatorRetargetingMode
- [ ] `Workspace.StreamOutBehavior`: StreamOutBehavior
- [ ] `Workspace.StreamingAdaptiveRadius`: bool
- [ ] `Workspace.StreamingEnabled`: bool
- [ ] `Workspace.StreamingIntegrityMode`: StreamingIntegrityMode
- [ ] `Workspace.StreamingMinRadius`: int
- [ ] `Workspace.StreamingTargetRadius`: int
- [ ] `Workspace.Terrain`: Terrain  _(ReadOnly)_
- [ ] `Workspace:GetNumAwakeParts()` -> int
- [ ] `Workspace:GetPhysicsThrottling()` -> int
- [ ] `Workspace:GetRealPhysicsFPS()` -> double
- [ ] `Workspace:GetServerTimeNow()` -> double
- [ ] `Workspace:JoinToOutsiders(objects: Instances, jointType: JointCreationMode)` -> null
- [ ] `Workspace:PGSIsEnabled()` -> bool
- [ ] `Workspace:UnjoinFromOutsiders(objects: Instances)` -> null

### Terrain (eredita da BasePart) _NotCreatable_
- [ ] `Terrain.Decoration`: bool = false
- [ ] `Terrain.GrassLength`: float = 0.699999988
- [ ] `Terrain.MaxExtents`: Region3int16  _(ReadOnly)_
- [ ] `Terrain.WaterColor`: Color3 = 0.05, 0.33, 0.36
- [ ] `Terrain.WaterReflectance`: float = 1
- [ ] `Terrain.WaterTransparency`: float = 0.300000012
- [ ] `Terrain.WaterWaveSize`: float = 0.150000006
- [ ] `Terrain.WaterWaveSpeed`: float = 10
- [ ] `Terrain:CellCenterToWorld(x: int, y: int, z: int)` -> Vector3
- [ ] `Terrain:CellCornerToWorld(x: int, y: int, z: int)` -> Vector3
- [ ] `Terrain:Clear()` -> null
- [ ] `Terrain:ClearVoxelsAsync_beta(region: Region3, channelIds: Array)` -> null  _(CustomLuaState)_
- [ ] `Terrain:CopyRegion(region: Region3int16)` -> TerrainRegion
- [ ] `Terrain:CountCells()` -> int
- [ ] `Terrain:FillBall(center: Vector3, radius: float, material: Material)` -> null
- [ ] `Terrain:FillBlock(cframe: CFrame, size: Vector3, material: Material)` -> null
- [ ] `Terrain:FillCylinder(cframe: CFrame, height: float, radius: float, material: Material)` -> null
- [ ] `Terrain:FillRegion(region: Region3, resolution: float, material: Material)` -> null
- [ ] `Terrain:FillWedge(cframe: CFrame, size: Vector3, material: Material)` -> null
- [ ] `Terrain:GetMaterialColor(material: Material)` -> Color3
- [ ] `Terrain:GetMaterialSlot(slotIndex: int)` -> Tuple
- [ ] `Terrain:IterateVoxelsAsync_beta(region: Region3, resolution: int, channelIds: Array)` -> TerrainIterateOperation  _(CustomLuaState)_
- [ ] `Terrain:ModifyVoxelsAsync_beta(region: Region3, resolution: int, channelIds: Array)` -> TerrainModifyOperation  _(CustomLuaState)_
- [ ] `Terrain:PasteRegion(region: TerrainRegion, corner: Vector3int16, pasteEmptyCells: bool)` -> null
- [ ] `Terrain:ReadVoxelChannels(region: Region3, resolution: float, channelIds: Array)` -> Dictionary  _(CustomLuaState)_
- [ ] `Terrain:ReadVoxels(region: Region3, resolution: float)` -> Tuple  _(CustomLuaState)_
- [ ] `Terrain:ReadVoxelsAsync_beta(region: Region3, resolution: int, channelIds: Array)` -> TerrainReadOperation  _(CustomLuaState)_
- [ ] `Terrain:ReplaceMaterial(region: Region3, resolution: float, sourceMaterial: Material, targetMaterial: Material)` -> null
- [ ] `Terrain:ResetMaterialSlot(slotIndex: int)` -> null
- [ ] `Terrain:SetMaterialColor(material: Material, value: Color3)` -> null
- [ ] `Terrain:SetMaterialSlot(slotIndex: int, baseMaterial: Material, materialVariant: string, color: Color3)` -> null
- [ ] `Terrain:WorldToCell(position: Vector3)` -> Vector3
- [ ] `Terrain:WorldToCellPreferEmpty(position: Vector3)` -> Vector3
- [ ] `Terrain:WorldToCellPreferSolid(position: Vector3)` -> Vector3
- [ ] `Terrain:WriteVoxelChannels(region: Region3, resolution: float, channels: Dictionary)` -> null  _(CustomLuaState)_
- [ ] `Terrain:WriteVoxels(region: Region3, resolution: float, materials: Array, occupancy: Array)` -> null  _(CustomLuaState)_
- [ ] `Terrain:WriteVoxelsAsync_beta(region: Region3, resolution: int, channelIds: Array)` -> TerrainWriteOperation  _(CustomLuaState)_

### Decal (eredita da FaceInstance)
- [ ] `Decal.AutoLocalize`: bool = true
- [ ] `Decal.Color3`: Color3 = 1, 1, 1
- [ ] `Decal.ColorMapContent`: Content
- [ ] `Decal.ColorMap`: ContentId = 
- [ ] `Decal.EmissiveMaskContent`: Content
- [ ] `Decal.EmissiveStrength`: float = 1
- [ ] `Decal.EmissiveTint`: Color3 = 1, 1, 1
- [ ] `Decal.MetalnessMapContent`: Content
- [ ] `Decal.NormalMapContent`: Content
- [ ] `Decal.ResampleMode`: ResamplerMode = Default
- [ ] `Decal.Rotation`: float = 0
- [ ] `Decal.RoughnessMapContent`: Content
- [ ] `Decal.TextureContent`: Content
- [ ] `Decal.Texture`: ContentId = 
- [ ] `Decal.Transparency`: float = 0
- [ ] `Decal.UVOffset`: Vector2 = 0, 0
- [ ] `Decal.UVScale`: Vector2 = 1, 1
- [ ] `Decal.ZIndex`: int = 1

### Texture (eredita da Decal)
- [ ] `Texture.OffsetStudsU`: float = 0
- [ ] `Texture.OffsetStudsV`: float = 0
- [ ] `Texture.StudsPerTileU`: float = 2
- [ ] `Texture.StudsPerTileV`: float = 2

### DataModelMesh (eredita da Instance) _NotCreatable_
- [ ] `DataModelMesh.Offset`: Vector3
- [ ] `DataModelMesh.Scale`: Vector3
- [ ] `DataModelMesh.VertexColor`: Vector3

### FileMesh (eredita da DataModelMesh)
- [ ] `FileMesh.MeshContent`: Content
- [ ] `FileMesh.MeshId`: ContentId = 
- [ ] `FileMesh.TextureContent`: Content
- [ ] `FileMesh.TextureId`: ContentId = 

### SpecialMesh (eredita da FileMesh)
- [ ] `SpecialMesh.MeshType`: MeshType = Head

### BlockMesh (eredita da BevelMesh)
_(nessun membro proprio: vedi la classe da cui eredita)_

### SurfaceAppearance (eredita da Instance)
- [ ] `SurfaceAppearance.AlphaMode`: AlphaMode = Overlay
- [ ] `SurfaceAppearance.ColorMapContent`: Content
- [ ] `SurfaceAppearance.Color`: Color3 = 1, 1, 1
- [ ] `SurfaceAppearance.EmissiveMaskContent`: Content
- [ ] `SurfaceAppearance.EmissiveStrength`: float = 1
- [ ] `SurfaceAppearance.EmissiveTint`: Color3 = 1, 1, 1
- [ ] `SurfaceAppearance.MetalnessMapContent`: Content
- [ ] `SurfaceAppearance.NormalMapContent`: Content
- [ ] `SurfaceAppearance.ResampleMode`: ResamplerMode = Default
- [ ] `SurfaceAppearance.RoughnessMapContent`: Content
- [ ] `SurfaceAppearance.SurfaceSlot`: int = -1

### MaterialVariant (eredita da Instance)
- [ ] `MaterialVariant.AlphaMode`: AlphaMode = Opaque
- [ ] `MaterialVariant.BaseMaterial`: Material = Plastic
- [ ] `MaterialVariant.CustomPhysicalProperties`: PhysicalProperties = default
- [ ] `MaterialVariant.EmissiveStrength`: float = 1
- [ ] `MaterialVariant.EmissiveTint`: Color3 = 1, 1, 1
- [ ] `MaterialVariant.MaterialPattern`: MaterialPattern = Regular
- [ ] `MaterialVariant.StudsPerTile`: float = 10

## P4 - Fisica e movimento

### Attachment (eredita da Instance)
- [ ] `Attachment.Axis`: Vector3 = 1, 0, 0
- [ ] `Attachment.CFrame`: CFrame = 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1
- [ ] `Attachment.SecondaryAxis`: Vector3 = 0, 1, 0
- [ ] `Attachment.Visible`: bool = false
- [ ] `Attachment.WorldAxis`: Vector3 = 1, 0, 0
- [ ] `Attachment.WorldCFrame`: CFrame = 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1
- [ ] `Attachment.WorldSecondaryAxis`: Vector3 = 0, 1, 0
- [ ] `Attachment:GetConstraints()` -> Instances

### Bone (eredita da Attachment)
- [ ] `Bone.Transform`: CFrame = 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1
- [ ] `Bone.TransformedWorldCFrame`: CFrame = 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1  _(ReadOnly)_

### Constraint (eredita da Instance) _NotCreatable_
- [ ] `Constraint.Active`: bool  _(ReadOnly)_
- [ ] `Constraint.Attachment0`: Attachment
- [ ] `Constraint.Attachment1`: Attachment
- [ ] `Constraint.Color`: BrickColor
- [ ] `Constraint.Enabled`: bool
- [ ] `Constraint.Visible`: bool

### WeldConstraint (eredita da Instance)
- [ ] `WeldConstraint.Active`: bool = false  _(ReadOnly)_
- [ ] `WeldConstraint.Enabled`: bool = true
- [ ] `WeldConstraint.Part0`: BasePart
- [ ] `WeldConstraint.Part1`: BasePart

### NoCollisionConstraint (eredita da Instance)
- [ ] `NoCollisionConstraint.Enabled`: bool = true
- [ ] `NoCollisionConstraint.Part0`: BasePart
- [ ] `NoCollisionConstraint.Part1`: BasePart

### JointInstance (eredita da Instance) _NotCreatable_
- [ ] `JointInstance.Active`: bool  _(ReadOnly)_
- [ ] `JointInstance.C0`: CFrame
- [ ] `JointInstance.C1`: CFrame
- [ ] `JointInstance.Enabled`: bool
- [ ] `JointInstance.Part0`: BasePart
- [ ] `JointInstance.Part1`: BasePart

### Weld (eredita da JointInstance)
_(nessun membro proprio: vedi la classe da cui eredita)_

### Motor6D (eredita da Motor)
_(nessun membro proprio: vedi la classe da cui eredita)_

### Motor (eredita da JointInstance)
- [ ] `Motor.CurrentAngle`: float = 0
- [ ] `Motor.DesiredAngle`: float = 0
- [ ] `Motor.MaxVelocity`: float = 0
- [ ] `Motor:SetDesiredAngle(value: float)` -> null

### HingeConstraint (eredita da Constraint)
- [ ] `HingeConstraint.ActuatorType`: ActuatorType = None
- [ ] `HingeConstraint.AngularResponsiveness`: float = 45
- [ ] `HingeConstraint.AngularSpeed`: float = 0
- [ ] `HingeConstraint.AngularVelocity`: float = 0
- [ ] `HingeConstraint.CurrentAngle`: float = 0  _(ReadOnly)_
- [ ] `HingeConstraint.LimitsEnabled`: bool = false
- [ ] `HingeConstraint.LowerAngle`: float = -45
- [ ] `HingeConstraint.MotorMaxAcceleration`: float = 500000
- [ ] `HingeConstraint.MotorMaxTorque`: float = 0
- [ ] `HingeConstraint.Radius`: float = 0.150000006
- [ ] `HingeConstraint.Restitution`: float = 0
- [ ] `HingeConstraint.ServoMaxTorque`: float = 0
- [ ] `HingeConstraint.TargetAngle`: float = 0
- [ ] `HingeConstraint.UpperAngle`: float = 45

### PrismaticConstraint (eredita da SlidingBallConstraint)
_(nessun membro proprio: vedi la classe da cui eredita)_

### SlidingBallConstraint (eredita da Constraint) _NotCreatable_
- [ ] `SlidingBallConstraint.ActuatorType`: ActuatorType
- [ ] `SlidingBallConstraint.CurrentPosition`: float  _(ReadOnly)_
- [ ] `SlidingBallConstraint.LimitsEnabled`: bool
- [ ] `SlidingBallConstraint.LinearResponsiveness`: float
- [ ] `SlidingBallConstraint.LowerLimit`: float
- [ ] `SlidingBallConstraint.MotorMaxAcceleration`: float
- [ ] `SlidingBallConstraint.MotorMaxForce`: float
- [ ] `SlidingBallConstraint.Restitution`: float
- [ ] `SlidingBallConstraint.ServoMaxForce`: float
- [ ] `SlidingBallConstraint.Size`: float
- [ ] `SlidingBallConstraint.Speed`: float
- [ ] `SlidingBallConstraint.TargetPosition`: float
- [ ] `SlidingBallConstraint.UpperLimit`: float
- [ ] `SlidingBallConstraint.Velocity`: float

### CylindricalConstraint (eredita da SlidingBallConstraint)
- [ ] `CylindricalConstraint.AngularActuatorType`: ActuatorType = None
- [ ] `CylindricalConstraint.AngularLimitsEnabled`: bool = false
- [ ] `CylindricalConstraint.AngularResponsiveness`: float = 45
- [ ] `CylindricalConstraint.AngularRestitution`: float = 0
- [ ] `CylindricalConstraint.AngularSpeed`: float = 0
- [ ] `CylindricalConstraint.AngularVelocity`: float = 0
- [ ] `CylindricalConstraint.CurrentAngle`: float = 0  _(ReadOnly)_
- [ ] `CylindricalConstraint.InclinationAngle`: float = 0
- [ ] `CylindricalConstraint.LowerAngle`: float = -45
- [ ] `CylindricalConstraint.MotorMaxAngularAcceleration`: float = 500000
- [ ] `CylindricalConstraint.MotorMaxTorque`: float = 0
- [ ] `CylindricalConstraint.RotationAxisVisible`: bool = false
- [ ] `CylindricalConstraint.ServoMaxTorque`: float = 0
- [ ] `CylindricalConstraint.TargetAngle`: float = 0
- [ ] `CylindricalConstraint.UpperAngle`: float = 45
- [ ] `CylindricalConstraint.WorldRotationAxis`: Vector3 = 1, 0, 0  _(ReadOnly)_

### BallSocketConstraint (eredita da Constraint)
- [ ] `BallSocketConstraint.LimitsEnabled`: bool = false
- [ ] `BallSocketConstraint.MaxFrictionTorque`: float = 0
- [ ] `BallSocketConstraint.Radius`: float = 0.150000006
- [ ] `BallSocketConstraint.Restitution`: float = 0
- [ ] `BallSocketConstraint.TwistLimitsEnabled`: bool = false
- [ ] `BallSocketConstraint.TwistLowerAngle`: float = -45
- [ ] `BallSocketConstraint.TwistUpperAngle`: float = 45
- [ ] `BallSocketConstraint.UpperAngle`: float = 45

### UniversalConstraint (eredita da Constraint)
- [ ] `UniversalConstraint.LimitsEnabled`: bool = false
- [ ] `UniversalConstraint.MaxAngle`: float = 45
- [ ] `UniversalConstraint.Radius`: float = 0.200000003
- [ ] `UniversalConstraint.Restitution`: float = 0

### PlaneConstraint (eredita da Constraint)
_(nessun membro proprio: vedi la classe da cui eredita)_

### RopeConstraint (eredita da Constraint)
- [ ] `RopeConstraint.CurrentDistance`: float = 0  _(ReadOnly)_
- [ ] `RopeConstraint.Length`: float = 5
- [ ] `RopeConstraint.Restitution`: float = 0
- [ ] `RopeConstraint.Thickness`: float = 0.100000001
- [ ] `RopeConstraint.WinchEnabled`: bool = false
- [ ] `RopeConstraint.WinchForce`: float = 10000
- [ ] `RopeConstraint.WinchResponsiveness`: float = 45
- [ ] `RopeConstraint.WinchSpeed`: float = 2
- [ ] `RopeConstraint.WinchTarget`: float = 5

### RodConstraint (eredita da Constraint)
- [ ] `RodConstraint.CurrentDistance`: float = 0  _(ReadOnly)_
- [ ] `RodConstraint.Length`: float = 5
- [ ] `RodConstraint.LimitAngle0`: float = 90
- [ ] `RodConstraint.LimitAngle1`: float = 90
- [ ] `RodConstraint.LimitsEnabled`: bool = false
- [ ] `RodConstraint.Thickness`: float = 0.100000001

### SpringConstraint (eredita da Constraint)
- [ ] `SpringConstraint.Coils`: float = 3
- [ ] `SpringConstraint.CurrentLength`: float = 0  _(ReadOnly)_
- [ ] `SpringConstraint.Damping`: float = 0
- [ ] `SpringConstraint.FreeLength`: float = 1
- [ ] `SpringConstraint.LimitsEnabled`: bool = false
- [ ] `SpringConstraint.MaxForce`: float = INF
- [ ] `SpringConstraint.MaxLength`: float = 5
- [ ] `SpringConstraint.MinLength`: float = 0
- [ ] `SpringConstraint.Radius`: float = 0.400000006
- [ ] `SpringConstraint.Stiffness`: float = 0
- [ ] `SpringConstraint.Thickness`: float = 0.100000001

### LinearVelocity (eredita da Constraint)
- [ ] `LinearVelocity.ForceLimitMode`: ForceLimitMode = Magnitude
- [ ] `LinearVelocity.ForceLimitsEnabled`: bool = true
- [ ] `LinearVelocity.LineDirection`: Vector3 = 1, 0, 0
- [ ] `LinearVelocity.LineVelocity`: float = -0
- [ ] `LinearVelocity.MaxAxesForce`: Vector3 = 1000, 1000, 1000
- [ ] `LinearVelocity.MaxForce`: float = 1000
- [ ] `LinearVelocity.MaxPlanarAxesForce`: Vector2 = 1000, 1000
- [ ] `LinearVelocity.PlaneVelocity`: Vector2 = 0, 0
- [ ] `LinearVelocity.PrimaryTangentAxis`: Vector3 = 1, 0, 0
- [ ] `LinearVelocity.ReactionForceEnabled`: bool = true
- [ ] `LinearVelocity.RelativeTo`: ActuatorRelativeTo = World
- [ ] `LinearVelocity.SecondaryTangentAxis`: Vector3 = 0, 1, 0
- [ ] `LinearVelocity.VectorVelocity`: Vector3 = 0, 0, 0
- [ ] `LinearVelocity.VelocityConstraintMode`: VelocityConstraintMode = Vector

### AngularVelocity (eredita da Constraint)
- [ ] `AngularVelocity.AngularVelocity`: Vector3 = 0, 0, 0
- [ ] `AngularVelocity.MaxTorque`: float = 0
- [ ] `AngularVelocity.ReactionTorqueEnabled`: bool = false
- [ ] `AngularVelocity.RelativeTo`: ActuatorRelativeTo = World

### AlignPosition (eredita da Constraint)
- [ ] `AlignPosition.ApplyAtCenterOfMass`: bool = false
- [ ] `AlignPosition.ForceLimitMode`: ForceLimitMode = Magnitude
- [ ] `AlignPosition.ForceRelativeTo`: ActuatorRelativeTo = World
- [ ] `AlignPosition.MaxAxesForce`: Vector3 = 10000, 10000, 10000
- [ ] `AlignPosition.MaxForce`: float = 10000
- [ ] `AlignPosition.MaxVelocity`: float = INF
- [ ] `AlignPosition.Mode`: PositionAlignmentMode = TwoAttachment
- [ ] `AlignPosition.Position`: Vector3 = 0, 0, 0
- [ ] `AlignPosition.ReactionForceEnabled`: bool = false
- [ ] `AlignPosition.Responsiveness`: float = 10
- [ ] `AlignPosition.RigidityEnabled`: bool = false

### AlignOrientation (eredita da Constraint)
- [ ] `AlignOrientation.AlignType`: AlignType = AllAxes
- [ ] `AlignOrientation.CFrame`: CFrame = 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1
- [ ] `AlignOrientation.LookAtPosition`: Vector3 = 0, 0, 0
- [ ] `AlignOrientation.MaxAngularVelocity`: float = INF
- [ ] `AlignOrientation.MaxTorque`: float = 10000
- [ ] `AlignOrientation.Mode`: OrientationAlignmentMode = TwoAttachment
- [ ] `AlignOrientation.PrimaryAxisOnly`: bool = false
- [ ] `AlignOrientation.PrimaryAxis`: Vector3 = 1, 0, 0
- [ ] `AlignOrientation.ReactionTorqueEnabled`: bool = false
- [ ] `AlignOrientation.Responsiveness`: float = 10
- [ ] `AlignOrientation.RigidityEnabled`: bool = false
- [ ] `AlignOrientation.SecondaryAxis`: Vector3 = 0, 1, 0

### VectorForce (eredita da Constraint)
- [ ] `VectorForce.ApplyAtCenterOfMass`: bool = false
- [ ] `VectorForce.Force`: Vector3 = 1000, 0, 0
- [ ] `VectorForce.RelativeTo`: ActuatorRelativeTo = Attachment0

### Torque (eredita da Constraint)
- [ ] `Torque.RelativeTo`: ActuatorRelativeTo = Attachment0
- [ ] `Torque.Torque`: Vector3 = 0, 0, 0

### LineForce (eredita da Constraint)
- [ ] `LineForce.ApplyAtCenterOfMass`: bool = false
- [ ] `LineForce.InverseSquareLaw`: bool = false
- [ ] `LineForce.Magnitude`: float = 1000
- [ ] `LineForce.MaxForce`: float = INF
- [ ] `LineForce.ReactionForceEnabled`: bool = false

### BodyMover (eredita da Instance) _NotCreatable, Deprecated_
_(nessun membro proprio: vedi la classe da cui eredita)_

### BodyVelocity (eredita da BodyMover) _Deprecated_
- [ ] `BodyVelocity.MaxForce`: Vector3 = 4000, 4000, 4000
- [ ] `BodyVelocity.P`: float = 1250
- [ ] `BodyVelocity.Velocity`: Vector3 = 0, 2, 0
- [ ] `BodyVelocity:GetLastForce()` -> Vector3

### BodyPosition (eredita da BodyMover) _Deprecated_
- [ ] `BodyPosition.D`: float = 1250
- [ ] `BodyPosition.MaxForce`: Vector3 = 4000, 4000, 4000
- [ ] `BodyPosition.P`: float = 10000
- [ ] `BodyPosition.Position`: Vector3 = 0, 50, 0
- [ ] `BodyPosition.ReachedTarget` (evento: nessun argomento)
- [ ] `BodyPosition:GetLastForce()` -> Vector3

### BodyGyro (eredita da BodyMover) _Deprecated_
- [ ] `BodyGyro.CFrame`: CFrame = 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1
- [ ] `BodyGyro.D`: float = 500
- [ ] `BodyGyro.MaxTorque`: Vector3 = 400000, 0, 400000
- [ ] `BodyGyro.P`: float = 3000

### BodyForce (eredita da BodyMover) _Deprecated_
- [ ] `BodyForce.Force`: Vector3 = 0, 1, 0

### BodyAngularVelocity (eredita da BodyMover) _Deprecated_
- [ ] `BodyAngularVelocity.AngularVelocity`: Vector3 = 0, 2, 0
- [ ] `BodyAngularVelocity.MaxTorque`: Vector3 = 4000, 4000, 4000
- [ ] `BodyAngularVelocity.P`: float = 1250

### BodyThrust (eredita da BodyMover) _Deprecated_
- [ ] `BodyThrust.Force`: Vector3 = 0, 1, 0
- [ ] `BodyThrust.Location`: Vector3 = 0, 0, 0

### RocketPropulsion (eredita da BodyMover) _Deprecated_
- [ ] `RocketPropulsion.CartoonFactor`: float = 0.699999988
- [ ] `RocketPropulsion.MaxSpeed`: float = 30
- [ ] `RocketPropulsion.MaxThrust`: float = 4000
- [ ] `RocketPropulsion.MaxTorque`: Vector3 = 400000, 400000, 0
- [ ] `RocketPropulsion.ReachedTarget` (evento: nessun argomento)
- [ ] `RocketPropulsion.TargetOffset`: Vector3 = 0, 0, 0
- [ ] `RocketPropulsion.TargetRadius`: float = 4
- [ ] `RocketPropulsion.Target`: BasePart
- [ ] `RocketPropulsion.ThrustD`: float = 0.00100000005
- [ ] `RocketPropulsion.ThrustP`: float = 5
- [ ] `RocketPropulsion.TurnD`: float = 500
- [ ] `RocketPropulsion.TurnP`: float = 3000
- [ ] `RocketPropulsion:Abort()` -> null
- [ ] `RocketPropulsion:Fire()` -> null

### PhysicsService (eredita da Instance) _NotCreatable, Service_
- [ ] `PhysicsService:CollisionGroupSetCollidable(name1: string, name2: string, collidable: bool)` -> null
- [ ] `PhysicsService:CollisionGroupsAreCollidable(name1: string, name2: string)` -> bool
- [ ] `PhysicsService:GetMaxCollisionGroups()` -> int
- [ ] `PhysicsService:GetRegisteredCollisionGroups()` -> Array
- [ ] `PhysicsService:IsCollisionGroupRegistered(name: string)` -> bool
- [ ] `PhysicsService:RegisterCollisionGroup(name: string)` -> null
- [ ] `PhysicsService:RenameCollisionGroup(from: string, to: string)` -> null
- [ ] `PhysicsService:UnregisterCollisionGroup(name: string)` -> null

### Seat (eredita da Part)
- [ ] `Seat.Disabled`: bool = false
- [ ] `Seat.Occupant`: Humanoid  _(ReadOnly)_
- [ ] `Seat:Sit(humanoid: Instance)` -> null

### VehicleSeat (eredita da BasePart)
- [ ] `VehicleSeat.AreHingesDetected`: int = 0  _(ReadOnly)_
- [ ] `VehicleSeat.Disabled`: bool = false
- [ ] `VehicleSeat.HeadsUpDisplay`: bool = true
- [ ] `VehicleSeat.MaxSpeed`: float = 25
- [ ] `VehicleSeat.Occupant`: Humanoid  _(ReadOnly)_
- [ ] `VehicleSeat.SteerFloat`: float = 0
- [ ] `VehicleSeat.Steer`: int = 0
- [ ] `VehicleSeat.ThrottleFloat`: float = 0
- [ ] `VehicleSeat.Throttle`: int = 0
- [ ] `VehicleSeat.Torque`: float = 10
- [ ] `VehicleSeat.TurnSpeed`: float = 1
- [ ] `VehicleSeat:Sit(humanoid: Instance)` -> null

## P5 - Personaggi, NPC e animazioni

### Humanoid (eredita da Instance)
- [ ] `Humanoid.ApplyDescriptionFinished` (evento: description: HumanoidDescription)
- [ ] `Humanoid.AutoJumpEnabled`: bool = true
- [ ] `Humanoid.AutoRotate`: bool = true
- [ ] `Humanoid.AutomaticScalingEnabled`: bool = true
- [ ] `Humanoid.BreakJointsOnDeath`: bool = true
- [ ] `Humanoid.CameraOffset`: Vector3 = 0, 0, 0
- [ ] `Humanoid.Climbing` (evento: speed: float)
- [ ] `Humanoid.Died` (evento: nessun argomento)
- [ ] `Humanoid.DisplayDistanceType`: HumanoidDisplayDistanceType = Viewer
- [ ] `Humanoid.DisplayName`: string = 
- [ ] `Humanoid.EvaluateStateMachine`: bool = true
- [ ] `Humanoid.FallingDown` (evento: active: bool)
- [ ] `Humanoid.FloorMaterial`: Material = Air  _(ReadOnly)_
- [ ] `Humanoid.FreeFalling` (evento: active: bool)
- [ ] `Humanoid.GettingUp` (evento: active: bool)
- [ ] `Humanoid.HealthChanged` (evento: health: float)
- [ ] `Humanoid.HealthDisplayDistance`: float = 100
- [ ] `Humanoid.HealthDisplayType`: HumanoidHealthDisplayType = DisplayWhenDamaged
- [ ] `Humanoid.Health`: float = 100
- [ ] `Humanoid.HipHeight`: float = 0
- [ ] `Humanoid.JumpHeight`: float = 7.19999981
- [ ] `Humanoid.JumpPower`: float = 50
- [ ] `Humanoid.Jump`: bool = false
- [ ] `Humanoid.Jumping` (evento: active: bool)
- [ ] `Humanoid.MaxHealth`: float = 100
- [ ] `Humanoid.MaxSlopeAngle`: float = 89
- [ ] `Humanoid.MoveDirection`: Vector3 = 0, 0, 0  _(ReadOnly)_
- [ ] `Humanoid.MoveToFinished` (evento: reached: bool)
- [ ] `Humanoid.NameDisplayDistance`: float = 100
- [ ] `Humanoid.NameOcclusion`: NameOcclusion = OccludeAll
- [ ] `Humanoid.PlatformStand`: bool = false
- [ ] `Humanoid.PlatformStanding` (evento: active: bool)
- [ ] `Humanoid.Ragdoll` (evento: active: bool)
- [ ] `Humanoid.RequiresNeck`: bool = true
- [ ] `Humanoid.RigType`: HumanoidRigType = R6
- [ ] `Humanoid.RootPart`: BasePart  _(ReadOnly)_
- [ ] `Humanoid.Running` (evento: speed: float)
- [ ] `Humanoid.SeatPart`: BasePart  _(ReadOnly)_
- [ ] `Humanoid.Seated` (evento: active: bool, currentSeatPart: BasePart)
- [ ] `Humanoid.Sit`: bool = false
- [ ] `Humanoid.StateChanged` (evento: old: HumanoidStateType, new: HumanoidStateType)
- [ ] `Humanoid.StateEnabledChanged` (evento: state: HumanoidStateType, isEnabled: bool)
- [ ] `Humanoid.Strafing` (evento: active: bool)
- [ ] `Humanoid.Swimming` (evento: speed: float)
- [ ] `Humanoid.TargetPoint`: Vector3 = 0, 0, 0
- [ ] `Humanoid.Touched` (evento: touchingPart: BasePart, humanoidPart: BasePart)
- [ ] `Humanoid.UseJumpPower`: bool = true
- [ ] `Humanoid.WalkSpeed`: float = 16
- [ ] `Humanoid.WalkToPart`: BasePart
- [ ] `Humanoid.WalkToPoint`: Vector3 = 0, 0, 0
- [ ] `Humanoid:AddAccessory(accessory: Instance)` -> null
- [ ] `Humanoid:ApplyAvatarRulesAsync(avatarRules: Variant)` -> null  _(Yields)_
- [ ] `Humanoid:ApplyDescriptionAsync(humanoidDescription: HumanoidDescription, assetTypeVerification: AssetTypeVerification = Default)` -> null  _(Yields)_
- [ ] `Humanoid:ApplyDescriptionResetAsync(humanoidDescription: HumanoidDescription, assetTypeVerification: AssetTypeVerification = Default)` -> null  _(Yields)_
- [ ] `Humanoid:BuildRigFromAttachments()` -> null
- [ ] `Humanoid:ChangeState(state: HumanoidStateType = None)` -> null
- [ ] `Humanoid:EquipTool(tool: Instance)` -> null
- [ ] `Humanoid:GetAccessories()` -> Array
- [ ] `Humanoid:GetAppliedDescription()` -> HumanoidDescription
- [ ] `Humanoid:GetBodyPartR15(part: Instance)` -> BodyPartR15
- [ ] `Humanoid:GetLimb(part: Instance)` -> Limb
- [ ] `Humanoid:GetMoveVelocity()` -> Vector3
- [ ] `Humanoid:GetRelativeVelocityAtFloor()` -> Vector3
- [ ] `Humanoid:GetState()` -> HumanoidStateType
- [ ] `Humanoid:GetStateEnabled(state: HumanoidStateType)` -> bool
- [ ] `Humanoid:Move(moveDirection: Vector3, relativeToCamera: bool = false)` -> null
- [ ] `Humanoid:MoveTo(location: Vector3, part: Instance = nil)` -> null
- [ ] `Humanoid:PlayEmoteAsync(emoteName: string)` -> bool  _(Yields)_
- [ ] `Humanoid:RemoveAccessories()` -> null
- [ ] `Humanoid:ReplaceBodyPartR15(bodyPart: BodyPartR15, part: BasePart)` -> bool
- [ ] `Humanoid:SetStateEnabled(state: HumanoidStateType, enabled: bool)` -> null
- [ ] `Humanoid:TakeDamage(amount: float)` -> null
- [ ] `Humanoid:UnequipTools()` -> null

### HumanoidDescription (eredita da Instance)
- [ ] `HumanoidDescription.BackAccessory`: string = 
- [ ] `HumanoidDescription.BodyTypeScale`: float = 0.300000012
- [ ] `HumanoidDescription.ClimbAnimation`: int64 = 0
- [ ] `HumanoidDescription.DepthScale`: float = 1
- [ ] `HumanoidDescription.EmotesChanged` (evento: newEmotes: Dictionary)
- [ ] `HumanoidDescription.EquippedEmotesChanged` (evento: newEquippedEmotes: Array)
- [ ] `HumanoidDescription.FaceAccessory`: string = 
- [ ] `HumanoidDescription.Face`: int64 = 0
- [ ] `HumanoidDescription.FallAnimation`: int64 = 0
- [ ] `HumanoidDescription.FrontAccessory`: string = 
- [ ] `HumanoidDescription.GraphicTShirt`: int64 = 0
- [ ] `HumanoidDescription.HairAccessory`: string = 
- [ ] `HumanoidDescription.HatAccessory`: string = 
- [ ] `HumanoidDescription.HeadColor`: Color3 = 0, 0, 0
- [ ] `HumanoidDescription.HeadScale`: float = 1
- [ ] `HumanoidDescription.Head`: int64 = 0
- [ ] `HumanoidDescription.HeightScale`: float = 1
- [ ] `HumanoidDescription.IdleAnimation`: int64 = 0
- [ ] `HumanoidDescription.JumpAnimation`: int64 = 0
- [ ] `HumanoidDescription.LeftArmColor`: Color3 = 0, 0, 0
- [ ] `HumanoidDescription.LeftArm`: int64 = 0
- [ ] `HumanoidDescription.LeftLegColor`: Color3 = 0, 0, 0
- [ ] `HumanoidDescription.LeftLeg`: int64 = 0
- [ ] `HumanoidDescription.MoodAnimation`: int64 = 0
- [ ] `HumanoidDescription.NeckAccessory`: string = 
- [ ] `HumanoidDescription.Pants`: int64 = 0
- [ ] `HumanoidDescription.ProportionScale`: float = 1
- [ ] `HumanoidDescription.RightArmColor`: Color3 = 0, 0, 0
- [ ] `HumanoidDescription.RightArm`: int64 = 0
- [ ] `HumanoidDescription.RightLegColor`: Color3 = 0, 0, 0
- [ ] `HumanoidDescription.RightLeg`: int64 = 0
- [ ] `HumanoidDescription.RunAnimation`: int64 = 0
- [ ] `HumanoidDescription.Shirt`: int64 = 0
- [ ] `HumanoidDescription.ShouldersAccessory`: string = 
- [ ] `HumanoidDescription.StaticFacialAnimation`: bool = false
- [ ] `HumanoidDescription.SwimAnimation`: int64 = 0
- [ ] `HumanoidDescription.TorsoColor`: Color3 = 0, 0, 0
- [ ] `HumanoidDescription.Torso`: int64 = 0
- [ ] `HumanoidDescription.UseAvatarSettings`: bool = false
- [ ] `HumanoidDescription.WaistAccessory`: string = 
- [ ] `HumanoidDescription.WalkAnimation`: int64 = 0
- [ ] `HumanoidDescription.WidthScale`: float = 1
- [ ] `HumanoidDescription:AddEmote(name: string, assetId: int64)` -> null
- [ ] `HumanoidDescription:GetAccessories(includeRigidAccessories: bool)` -> Array
- [ ] `HumanoidDescription:GetEmotes()` -> Dictionary
- [ ] `HumanoidDescription:GetEquippedEmotes()` -> Array
- [ ] `HumanoidDescription:RemoveEmote(name: string)` -> null
- [ ] `HumanoidDescription:SetAccessories(accessories: Array, includeRigidAccessories: bool)` -> null
- [ ] `HumanoidDescription:SetEmotes(emotes: Dictionary)` -> null
- [ ] `HumanoidDescription:SetEquippedEmotes(equippedEmotes: Array)` -> null

### AnimationController (eredita da Instance)
_(nessun membro proprio: vedi la classe da cui eredita)_

### Animator (eredita da Instance)
- [ ] `Animator.AnimationPlayed` (evento: animationTrack: AnimationTrack)
- [ ] `Animator.EvaluationThrottled`: bool = false  _(ReadOnly)_
- [ ] `Animator.PreferLodEnabled`: bool = true
- [ ] `Animator.RootMotionWeight`: float = 0  _(ReadOnly)_
- [ ] `Animator.RootMotion`: CFrame = 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1  _(ReadOnly)_
- [ ] `Animator:ApplyJointVelocities(motors: Variant)` -> null
- [ ] `Animator:GetPlayingAnimationTracks()` -> Array
- [ ] `Animator:GetTrackByAnimationId(animationId: ContentId)` -> AnimationTrack
- [ ] `Animator:LoadAnimation(animation: Animation)` -> AnimationTrack
- [ ] `Animator:RegisterEvaluationParallelCallback(callback: Function)` -> null
- [ ] `Animator:StepAnimations(deltaTime: float)` -> null

### Animation (eredita da Instance)
- [ ] `Animation.AnimationContent`: Content
- [ ] `Animation.AnimationId`: ContentId = 

### AnimationTrack (eredita da Instance) _NotCreatable_
- [ ] `AnimationTrack.Animation`: Animation  _(ReadOnly)_
- [ ] `AnimationTrack.DidLoop` (evento: nessun argomento)
- [ ] `AnimationTrack.Ended` (evento: nessun argomento)
- [ ] `AnimationTrack.IsPlaying`: bool  _(ReadOnly)_
- [ ] `AnimationTrack.KeyframeReached` (evento: keyframeName: string)
- [ ] `AnimationTrack.Length`: float  _(ReadOnly)_
- [ ] `AnimationTrack.Looped`: bool
- [ ] `AnimationTrack.Priority`: AnimationPriority
- [ ] `AnimationTrack.Speed`: float  _(ReadOnly)_
- [ ] `AnimationTrack.Stopped` (evento: nessun argomento)
- [ ] `AnimationTrack.TimePosition`: float
- [ ] `AnimationTrack.WeightCurrent`: float  _(ReadOnly)_
- [ ] `AnimationTrack.WeightTarget`: float  _(ReadOnly)_
- [ ] `AnimationTrack:AdjustSpeed(speed: float = 1)` -> null  _(CustomLuaState)_
- [ ] `AnimationTrack:AdjustWeight(weight: float = 1, fadeTime: float = 0.100000001)` -> null  _(CustomLuaState)_
- [ ] `AnimationTrack:GetMarkerReachedSignal(name: string)` -> RBXScriptSignal
- [ ] `AnimationTrack:GetParameter(key: string)` -> Variant
- [ ] `AnimationTrack:GetParameterDefaults()` -> Dictionary
- [ ] `AnimationTrack:GetTargetInstance(name: string)` -> Instance
- [ ] `AnimationTrack:GetTargetNames()` -> Array
- [ ] `AnimationTrack:GetTimeOfKeyframe(keyframeName: string)` -> double
- [ ] `AnimationTrack:Play(fadeTime: float = 0.100000001, weight: float = 1, speed: float = 1)` -> null  _(CustomLuaState)_
- [ ] `AnimationTrack:SetParameter(key: string, value: Variant)` -> null
- [ ] `AnimationTrack:SetTargetInstance(name: string, target: Instance)` -> null
- [ ] `AnimationTrack:Stop(fadeTime: float = 0.100000001)` -> null  _(CustomLuaState)_

### Keyframe (eredita da Instance)
- [ ] `Keyframe.Time`: float = 0
- [ ] `Keyframe:AddMarker(marker: Instance)` -> null
- [ ] `Keyframe:AddPose(pose: Instance)` -> null
- [ ] `Keyframe:GetMarkers()` -> Instances
- [ ] `Keyframe:GetPoses()` -> Instances
- [ ] `Keyframe:RemoveMarker(marker: Instance)` -> null
- [ ] `Keyframe:RemovePose(pose: Instance)` -> null

### KeyframeSequence (eredita da AnimationClip)
- [ ] `KeyframeSequence:AddKeyframe(keyframe: Instance)` -> null
- [ ] `KeyframeSequence:GetKeyframes()` -> Instances
- [ ] `KeyframeSequence:RemoveKeyframe(keyframe: Instance)` -> null

### KeyframeMarker (eredita da Instance)
- [ ] `KeyframeMarker.Value`: string = 

### Player (eredita da Instance)
- [ ] `Player.AccountAge`: int  _(ReadOnly)_
- [ ] `Player.AgeChecked`: AgeCheckStatus
- [ ] `Player.AutoJumpEnabled`: bool
- [ ] `Player.CameraMaxZoomDistance`: float
- [ ] `Player.CameraMinZoomDistance`: float
- [ ] `Player.CameraMode`: CameraMode
- [ ] `Player.CanLoadCharacterAppearance`: bool
- [ ] `Player.CharacterAdded` (evento: character: Model)
- [ ] `Player.CharacterAppearanceId`: int64
- [ ] `Player.CharacterAppearanceLoaded` (evento: character: Model)
- [ ] `Player.CharacterRemoving` (evento: character: Model)
- [ ] `Player.Character`: Model
- [ ] `Player.Chatted` (evento: message: string, recipient: Player)
- [ ] `Player.DevCameraOcclusionMode`: DevCameraOcclusionMode
- [ ] `Player.DevComputerCameraMode`: DevComputerCameraMovementMode
- [ ] `Player.DevComputerMovementMode`: DevComputerMovementMode
- [ ] `Player.DevEnableMouseLock`: bool
- [ ] `Player.DevTouchCameraMode`: DevTouchCameraMovementMode
- [ ] `Player.DevTouchMovementMode`: DevTouchMovementMode
- [ ] `Player.DeviceTrustLevel`: PlayerDeviceTrustLevel  _(ReadOnly)_
- [ ] `Player.DisplayName`: string
- [ ] `Player.EmotesLoaded` (evento: nessun argomento)
- [ ] `Player.FollowUserId`: int64  _(ReadOnly)_
- [ ] `Player.FrustumStreaming`: FrustumStreamingMode
- [ ] `Player.HasRobloxSubscription`: bool
- [ ] `Player.HasVerifiedBadge`: bool
- [ ] `Player.HealthDisplayDistance`: float
- [ ] `Player.Idled` (evento: time: double)
- [ ] `Player.MembershipType`: MembershipType  _(ReadOnly)_
- [ ] `Player.NameDisplayDistance`: float
- [ ] `Player.Neutral`: bool
- [ ] `Player.OnTeleport` (evento: teleportState: TeleportState, placeId: int64, spawnName: string)
- [ ] `Player.ReplicationFocus`: Instance
- [ ] `Player.RespawnLocation`: SpawnLocation
- [ ] `Player.TeamColor`: BrickColor
- [ ] `Player.Team`: Team
- [ ] `Player.UserId`: int64
- [ ] `Player.User`: User
- [ ] `Player:AddReplicationFocus(part: BasePart)` -> null
- [ ] `Player:ClearCachedAvatarAppearance()` -> null
- [ ] `Player:ClearCharacterAppearance()` -> null
- [ ] `Player:DistanceFromCharacter(point: Vector3)` -> float
- [ ] `Player:GetCameraState()` -> Dictionary  _(CustomLuaState)_
- [ ] `Player:GetFriendsInServerAsync()` -> Array  _(Yields)_
- [ ] `Player:GetFriendsInUniverseAsync()` -> Array  _(Yields)_
- [ ] `Player:GetFriendsOnlineAsync(maxFriends: int = 200)` -> Array  _(Yields)_
- [ ] `Player:GetFriendsWhoPlayedAsync()` -> Array  _(Yields)_
- [ ] `Player:GetJoinData()` -> Dictionary  _(CustomLuaState)_
- [ ] `Player:GetMouse()` -> Mouse
- [ ] `Player:GetNetworkPing()` -> float
- [ ] `Player:HasAppearanceLoaded()` -> bool
- [ ] `Player:IsFriendsWithAsync(userId: User)` -> bool  _(Yields)_
- [ ] `Player:IsInGroupAsync(groupId: int64)` -> bool  _(Yields)_
- [ ] `Player:IsVerified(level: VerifiedLevel = Low)` -> bool
- [ ] `Player:Kick(message: string = )` -> null
- [ ] `Player:LoadCharacterAsync()` -> null  _(Yields)_
- [ ] `Player:LoadCharacterWithAvatarRulesAsync(avatarRules: Variant)` -> null  _(Yields)_
- [ ] `Player:LoadCharacterWithHumanoidDescriptionAsync(humanoidDescription: HumanoidDescription, assetTypeVerification: AssetTypeVerification = Default)` -> null  _(Yields)_
- [ ] `Player:Move(walkDirection: Vector3, relativeToCamera: bool = false)` -> null
- [ ] `Player:PromptAgeCheck()` -> null
- [ ] `Player:RemoveReplicationFocus(part: BasePart)` -> null
- [ ] `Player:RequestStreamAroundAsync(position: Vector3, timeOut: double = 0)` -> null  _(Yields)_

### Players (eredita da Instance) _NotCreatable, Service_
- [ ] `Players.BanningEnabled`: bool
- [ ] `Players.BubbleChat`: bool  _(ReadOnly)_
- [ ] `Players.CharacterAutoLoads`: bool
- [ ] `Players.ClassicChat`: bool  _(ReadOnly)_
- [ ] `Players.LocalPlayer`: Player  _(ReadOnly)_
- [ ] `Players.MaxPlayers`: int  _(ReadOnly)_
- [ ] `Players.PlayerAdded` (evento: player: Player)
- [ ] `Players.PlayerMembershipChanged` (evento: player: Player)
- [ ] `Players.PlayerRemoving` (evento: player: Player, reason: PlayerExitReason)
- [ ] `Players.PreferredPlayers`: int  _(ReadOnly)_
- [ ] `Players.RespawnTime`: float
- [ ] `Players.UseStrafingAnimations`: bool
- [ ] `Players.UserSubscriptionStatusChanged` (evento: user: Player, subscriptionId: string)
- [ ] `Players:BanAsync(config: Dictionary)` -> null  _(Yields)_
- [ ] `Players:CreateHumanoidModelFromDescriptionAsync(description: HumanoidDescription, rigType: HumanoidRigType, assetTypeVerification: AssetTypeVerification = Default)` -> Model  _(Yields)_
- [ ] `Players:CreateHumanoidModelFromUserIdAsync(userId: User)` -> Model  _(Yields)_
- [ ] `Players:GetBanHistoryAsync(userId: User)` -> BanHistoryPages  _(Yields)_
- [ ] `Players:GetCharacterAppearanceInfoAsync(userId: User)` -> Dictionary  _(Yields)_
- [ ] `Players:GetFriendsAsync(userId: User)` -> FriendPages  _(Yields)_
- [ ] `Players:GetHumanoidDescriptionFromOutfitIdAsync(outfitId: int64)` -> HumanoidDescription  _(Yields)_
- [ ] `Players:GetHumanoidDescriptionFromUserIdAsync(userId: User)` -> HumanoidDescription  _(Yields)_
- [ ] `Players:GetNameFromUserIdAsync(userId: User)` -> string  _(Yields)_
- [ ] `Players:GetPlayerByUserId(userId: User)` -> Player
- [ ] `Players:GetPlayerFromCharacter(character: Model)` -> Player
- [ ] `Players:GetPlayers()` -> Instances
- [ ] `Players:GetProfileConfigurationFromUserIdAsync(userId: User)` -> Dictionary  _(Yields)_
- [ ] `Players:GetUserIdFromNameAsync(userName: string)` -> int64  _(Yields)_
- [ ] `Players:GetUserThumbnailAsync(userId: User, thumbnailType: ThumbnailType, thumbnailSize: ThumbnailSize)` -> Tuple  _(Yields)_
- [ ] `Players:UnbanAsync(config: Dictionary)` -> null  _(Yields)_

### Team (eredita da Instance)
- [ ] `Team.AutoAssignable`: bool = true
- [ ] `Team.PlayerAdded` (evento: player: Player)
- [ ] `Team.PlayerRemoved` (evento: player: Player)
- [ ] `Team.TeamColor`: BrickColor
- [ ] `Team:GetPlayers()` -> Instances

### Teams (eredita da Instance) _NotCreatable, Service_
- [ ] `Teams:GetTeams()` -> Instances

### PathfindingService (eredita da Instance) _NotCreatable, Service_
- [ ] `PathfindingService:CreatePath(agentParameters: Dictionary = nil)` -> Path
- [ ] `PathfindingService:FindPathAsync(start: Vector3, finish: Vector3)` -> Path  _(Yields)_

### Path (eredita da Instance) _NotCreatable_
- [ ] `Path.Blocked` (evento: blockedWaypointIdx: int)
- [ ] `Path.Status`: PathStatus  _(ReadOnly)_
- [ ] `Path.Unblocked` (evento: unblockedWaypointIdx: int)
- [ ] `Path:CheckOcclusionAsync(start: int)` -> int  _(Yields)_
- [ ] `Path:ComputeAsync(start: Vector3, finish: Vector3)` -> null  _(Yields)_
- [ ] `Path:GetWaypoints()` -> Array

### PathfindingModifier (eredita da Instance)
- [ ] `PathfindingModifier.Label`: string = 
- [ ] `PathfindingModifier.PassThrough`: bool = false

### PathfindingLink (eredita da Instance)
- [ ] `PathfindingLink.Attachment0`: Attachment
- [ ] `PathfindingLink.Attachment1`: Attachment
- [ ] `PathfindingLink.IsBidirectional`: bool = true
- [ ] `PathfindingLink.Label`: string = 

### Clothing (eredita da CharacterAppearance) _NotCreatable_
- [ ] `Clothing.Color3`: Color3

### Shirt (eredita da Clothing)
- [ ] `Shirt.ShirtTemplateContent`: Content
- [ ] `Shirt.ShirtTemplate`: ContentId = 

### Pants (eredita da Clothing)
- [ ] `Pants.PantsTemplateContent`: Content
- [ ] `Pants.PantsTemplate`: ContentId = 

### ShirtGraphic (eredita da CharacterAppearance)
- [ ] `ShirtGraphic.Color3`: Color3 = 1, 1, 1
- [ ] `ShirtGraphic.Graphic`: ContentId = 
- [ ] `ShirtGraphic.TextureContent`: Content

### BodyColors (eredita da CharacterAppearance)
- [ ] `BodyColors.HeadColor3`: Color3 = 0.992157, 0.917647, 0.552941
- [ ] `BodyColors.HeadColor`: BrickColor
- [ ] `BodyColors.LeftArmColor3`: Color3 = 0.992157, 0.917647, 0.552941
- [ ] `BodyColors.LeftArmColor`: BrickColor
- [ ] `BodyColors.LeftLegColor3`: Color3 = 0.0509804, 0.411765, 0.67451
- [ ] `BodyColors.LeftLegColor`: BrickColor
- [ ] `BodyColors.RightArmColor3`: Color3 = 0.992157, 0.917647, 0.552941
- [ ] `BodyColors.RightArmColor`: BrickColor
- [ ] `BodyColors.RightLegColor3`: Color3 = 0.0509804, 0.411765, 0.67451
- [ ] `BodyColors.RightLegColor`: BrickColor
- [ ] `BodyColors.TorsoColor3`: Color3 = 0.156863, 0.498039, 0.278431
- [ ] `BodyColors.TorsoColor`: BrickColor

### Accoutrement (eredita da Instance)
- [ ] `Accoutrement.AttachmentPoint`: CFrame = 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1

### Accessory (eredita da Accoutrement)
- [ ] `Accessory.AccessoryType`: AccessoryType = Unknown

### CharacterMesh (eredita da CharacterAppearance)
- [ ] `CharacterMesh.BaseTextureContent`: Content
- [ ] `CharacterMesh.BaseTextureId`: int64 = 0
- [ ] `CharacterMesh.BodyPart`: BodyPart = Head
- [ ] `CharacterMesh.MeshContent`: Content
- [ ] `CharacterMesh.MeshId`: int64 = 0
- [ ] `CharacterMesh.OverlayTextureContent`: Content
- [ ] `CharacterMesh.OverlayTextureId`: int64 = 0

### StarterPlayer (eredita da Instance) _NotCreatable, Service_
- [ ] `StarterPlayer.AutoJumpEnabled`: bool = true
- [ ] `StarterPlayer.CameraMaxZoomDistance`: float = 400
- [ ] `StarterPlayer.CameraMinZoomDistance`: float = 0.5
- [ ] `StarterPlayer.CameraMode`: CameraMode = Classic
- [ ] `StarterPlayer.CharacterBreakJointsOnDeath`: bool = true
- [ ] `StarterPlayer.CharacterJumpHeight`: float = 7.19999981
- [ ] `StarterPlayer.CharacterJumpPower`: float = 50
- [ ] `StarterPlayer.CharacterMaxSlopeAngle`: float = 89
- [ ] `StarterPlayer.CharacterUseJumpPower`: bool = true
- [ ] `StarterPlayer.CharacterWalkSpeed`: float = 16
- [ ] `StarterPlayer.ClassicDeath`: bool = true
- [ ] `StarterPlayer.CreateDefaultPlayerModule`: bool = true
- [ ] `StarterPlayer.DevCameraOcclusionMode`: DevCameraOcclusionMode = Zoom
- [ ] `StarterPlayer.DevComputerCameraMovementMode`: DevComputerCameraMovementMode = UserChoice
- [ ] `StarterPlayer.DevComputerMovementMode`: DevComputerMovementMode = UserChoice
- [ ] `StarterPlayer.DevTouchCameraMovementMode`: DevTouchCameraMovementMode = UserChoice
- [ ] `StarterPlayer.DevTouchMovementMode`: DevTouchMovementMode = UserChoice
- [ ] `StarterPlayer.EnableMouseLockOption`: bool = true
- [ ] `StarterPlayer.HealthDisplayDistance`: float = 100
- [ ] `StarterPlayer.LoadCharacterAppearance`: bool = true
- [ ] `StarterPlayer.LuaCharacterController`: CharacterControlMode = Default
- [ ] `StarterPlayer.NameDisplayDistance`: float = 100
- [ ] `StarterPlayer.PlaceAvatarRules`: Instance
- [ ] `StarterPlayer.UserEmotesEnabled`: bool = true

### StarterPlayerScripts (eredita da Instance) _NotCreatable_
_(nessun membro proprio: vedi la classe da cui eredita)_

### StarterCharacterScripts (eredita da StarterPlayerScripts) _NotCreatable_
_(nessun membro proprio: vedi la classe da cui eredita)_

## P6 - Strumenti e combattimento

### BackpackItem (eredita da Model) _NotCreatable_
- [ ] `BackpackItem.TextureContent`: Content
- [ ] `BackpackItem.TextureId`: ContentId

### Tool (eredita da BackpackItem)
- [ ] `Tool.Activated` (evento: nessun argomento)
- [ ] `Tool.CanBeDropped`: bool = true
- [ ] `Tool.Deactivated` (evento: nessun argomento)
- [ ] `Tool.Enabled`: bool = true
- [ ] `Tool.Equipped` (evento: mouse: Mouse)
- [ ] `Tool.Grip`: CFrame = 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1
- [ ] `Tool.ManualActivationOnly`: bool = false
- [ ] `Tool.RequiresHandle`: bool = true
- [ ] `Tool.ToolTip`: string = 
- [ ] `Tool.Unequipped` (evento: nessun argomento)
- [ ] `Tool:Activate()` -> null
- [ ] `Tool:Deactivate()` -> null

### Backpack (eredita da Instance)
_(nessun membro proprio: vedi la classe da cui eredita)_

### StarterPack (eredita da Instance) _NotCreatable, Service_
_(nessun membro proprio: vedi la classe da cui eredita)_

### StarterGear (eredita da Instance)
- [ ] `StarterGear.Loaded`: bool = false  _(ReadOnly)_

### Explosion (eredita da Instance)
- [ ] `Explosion.BlastPressure`: float = 500000
- [ ] `Explosion.BlastRadius`: float = 4
- [ ] `Explosion.DestroyJointRadiusPercent`: float = 1
- [ ] `Explosion.ExplosionType`: ExplosionType = Craters
- [ ] `Explosion.Hit` (evento: part: BasePart, distance: float)
- [ ] `Explosion.Position`: Vector3 = 0, 0, 0
- [ ] `Explosion.TimeScale`: float = 1
- [ ] `Explosion.Visible`: bool = true

### ForceField (eredita da Instance)
- [ ] `ForceField.Visible`: bool = true

### Debris (eredita da Instance) _NotCreatable, Service_
- [ ] `Debris:AddItem(item: Instance, lifetime: double = 10)` -> null

### ProximityPrompt (eredita da Instance)
- [ ] `ProximityPrompt.ActionText`: string = Interact
- [ ] `ProximityPrompt.AutoLocalize`: bool = true
- [ ] `ProximityPrompt.ClickablePrompt`: bool = true
- [ ] `ProximityPrompt.Enabled`: bool = true
- [ ] `ProximityPrompt.Exclusivity`: ProximityPromptExclusivity = OnePerButton
- [ ] `ProximityPrompt.GamepadKeyCode`: KeyCode = ButtonX
- [ ] `ProximityPrompt.HoldDuration`: float = 0
- [ ] `ProximityPrompt.IndicatorHidden` (evento: nessun argomento)
- [ ] `ProximityPrompt.IndicatorShown` (evento: nessun argomento)
- [ ] `ProximityPrompt.KeyboardKeyCode`: KeyCode = E
- [ ] `ProximityPrompt.MaxActivationDistance`: float = 10
- [ ] `ProximityPrompt.MaxIndicatorDistance`: float = 0
- [ ] `ProximityPrompt.ObjectText`: string = 
- [ ] `ProximityPrompt.PromptButtonHoldBegan` (evento: playerWhoTriggered: Player)
- [ ] `ProximityPrompt.PromptButtonHoldEnded` (evento: playerWhoTriggered: Player)
- [ ] `ProximityPrompt.PromptHidden` (evento: nessun argomento)
- [ ] `ProximityPrompt.PromptShown` (evento: inputType: ProximityPromptInputType)
- [ ] `ProximityPrompt.RequiresLineOfSight`: bool = true
- [ ] `ProximityPrompt.RootLocalizationTable`: LocalizationTable
- [ ] `ProximityPrompt.Style`: ProximityPromptStyle = Default
- [ ] `ProximityPrompt.TriggerEnded` (evento: playerWhoTriggered: Player)
- [ ] `ProximityPrompt.Triggered` (evento: playerWhoTriggered: Player)
- [ ] `ProximityPrompt.UIOffset`: Vector2 = 0, 0
- [ ] `ProximityPrompt:InputHoldBegin()` -> null
- [ ] `ProximityPrompt:InputHoldEnd()` -> null

### ProximityPromptService (eredita da Instance) _Service_
- [ ] `ProximityPromptService.Enabled`: bool = true
- [ ] `ProximityPromptService.IndicatorHidden` (evento: prompt: ProximityPrompt)
- [ ] `ProximityPromptService.IndicatorShown` (evento: prompt: ProximityPrompt)
- [ ] `ProximityPromptService.MaxIndicatorsVisible`: int = 16
- [ ] `ProximityPromptService.MaxPromptsVisible`: int = 16
- [ ] `ProximityPromptService.PromptButtonHoldBegan` (evento: prompt: ProximityPrompt, playerWhoTriggered: Player)
- [ ] `ProximityPromptService.PromptButtonHoldEnded` (evento: prompt: ProximityPrompt, playerWhoTriggered: Player)
- [ ] `ProximityPromptService.PromptHidden` (evento: prompt: ProximityPrompt)
- [ ] `ProximityPromptService.PromptShown` (evento: prompt: ProximityPrompt, inputType: ProximityPromptInputType)
- [ ] `ProximityPromptService.PromptTriggerEnded` (evento: prompt: ProximityPrompt, playerWhoTriggered: Player)
- [ ] `ProximityPromptService.PromptTriggered` (evento: prompt: ProximityPrompt, playerWhoTriggered: Player)

### ClickDetector (eredita da Instance)
- [ ] `ClickDetector.CursorIconContent`: Content
- [ ] `ClickDetector.CursorIcon`: ContentId = 
- [ ] `ClickDetector.MaxActivationDistance`: float = 32
- [ ] `ClickDetector.MouseClick` (evento: playerWhoClicked: Player)
- [ ] `ClickDetector.MouseHoverEnter` (evento: playerWhoHovered: Player)
- [ ] `ClickDetector.MouseHoverLeave` (evento: playerWhoHovered: Player)
- [ ] `ClickDetector.RightMouseClick` (evento: playerWhoClicked: Player)

### DragDetector (eredita da ClickDetector)
- [ ] `DragDetector.ActivatedCursorIconContent`: Content
- [ ] `DragDetector.ActivatedCursorIcon`: ContentId = 
- [ ] `DragDetector.ApplyAtCenterOfMass`: bool = false
- [ ] `DragDetector.Axis`: Vector3 = 0, 1, 0
- [ ] `DragDetector.DragContinue` (evento: playerWhoDragged: Player, cursorRay: Ray, viewFrame: CFrame, vrInputFrame: OptionalCoordinateFrame, isModeSwitchKeyDown: bool)
- [ ] `DragDetector.DragEnd` (evento: playerWhoDragged: Player)
- [ ] `DragDetector.DragFrame`: CFrame = 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1
- [ ] `DragDetector.DragStart` (evento: playerWhoDragged: Player, cursorRay: Ray, viewFrame: CFrame, hitFrame: CFrame, clickedPart: BasePart, vrInputFrame: OptionalCoordinateFrame, isModeSwitchKeyDown: bool)
- [ ] `DragDetector.DragStyle`: DragDetectorDragStyle = TranslatePlane
- [ ] `DragDetector.Enabled`: bool = true
- [ ] `DragDetector.GamepadModeSwitchKeyCode`: KeyCode = ButtonR1
- [ ] `DragDetector.KeyboardModeSwitchKeyCode`: KeyCode = LeftControl
- [ ] `DragDetector.MaxDragAngle`: float = 0
- [ ] `DragDetector.MaxDragTranslation`: Vector3 = 0, 0, 0
- [ ] `DragDetector.MaxForce`: float = 10000000
- [ ] `DragDetector.MaxTorque`: float = 10000
- [ ] `DragDetector.MinDragAngle`: float = 0
- [ ] `DragDetector.MinDragTranslation`: Vector3 = 0, 0, 0
- [ ] `DragDetector.Orientation`: Vector3 = -0, 180, 90
- [ ] `DragDetector.PermissionPolicy`: DragDetectorPermissionPolicy = Everybody
- [ ] `DragDetector.ReferenceInstance`: Instance
- [ ] `DragDetector.ResponseStyle`: DragDetectorResponseStyle = Physical
- [ ] `DragDetector.Responsiveness`: float = 10
- [ ] `DragDetector.RunLocally`: bool = false
- [ ] `DragDetector.SecondaryAxis`: Vector3 = 1, 0, 0
- [ ] `DragDetector.TrackballRadialPullFactor`: float = 1
- [ ] `DragDetector.TrackballRollFactor`: float = 1
- [ ] `DragDetector.VRSwitchKeyCode`: KeyCode = ButtonL2
- [ ] `DragDetector.WorldAxis`: Vector3 = 0, 1, 0
- [ ] `DragDetector.WorldSecondaryAxis`: Vector3 = 1, 0, 0
- [ ] `DragDetector:AddConstraintFunction(priority: int, function: Function)` -> RBXScriptConnection
- [ ] `DragDetector:GetReferenceFrame()` -> CFrame
- [ ] `DragDetector:RestartDrag()` -> null
- [ ] `DragDetector:SetDragStyleFunction(function: Function)` -> null
- [ ] `DragDetector:SetPermissionPolicyFunction(function: Function)` -> null

### Mouse (eredita da Instance) _NotCreatable_
- [ ] `Mouse.Button1Down` (evento: nessun argomento)
- [ ] `Mouse.Button1Up` (evento: nessun argomento)
- [ ] `Mouse.Button2Down` (evento: nessun argomento)
- [ ] `Mouse.Button2Up` (evento: nessun argomento)
- [ ] `Mouse.Hit`: CFrame  _(ReadOnly)_
- [ ] `Mouse.IconContent`: Content
- [ ] `Mouse.Icon`: ContentId
- [ ] `Mouse.Idle` (evento: nessun argomento)
- [ ] `Mouse.Move` (evento: nessun argomento)
- [ ] `Mouse.Origin`: CFrame  _(ReadOnly)_
- [ ] `Mouse.TargetFilter`: Instance
- [ ] `Mouse.TargetSurface`: NormalId  _(ReadOnly)_
- [ ] `Mouse.Target`: BasePart  _(ReadOnly)_
- [ ] `Mouse.UnitRay`: Ray  _(ReadOnly)_
- [ ] `Mouse.ViewSizeX`: int  _(ReadOnly)_
- [ ] `Mouse.ViewSizeY`: int  _(ReadOnly)_
- [ ] `Mouse.WheelBackward` (evento: nessun argomento)
- [ ] `Mouse.WheelForward` (evento: nessun argomento)
- [ ] `Mouse.X`: int  _(ReadOnly)_
- [ ] `Mouse.Y`: int  _(ReadOnly)_

### PlayerMouse (eredita da Mouse) _NotCreatable_
_(nessun membro proprio: vedi la classe da cui eredita)_

## P7 - GUI

### GuiBase (eredita da Instance) _NotCreatable_
_(nessun membro proprio: vedi la classe da cui eredita)_

### GuiBase2d (eredita da GuiBase) _NotCreatable_
- [ ] `GuiBase2d.AbsolutePosition`: Vector2  _(ReadOnly)_
- [ ] `GuiBase2d.AbsoluteRotation`: float  _(ReadOnly)_
- [ ] `GuiBase2d.AbsoluteSize`: Vector2  _(ReadOnly)_
- [ ] `GuiBase2d.AutoLocalize`: bool
- [ ] `GuiBase2d.RootLocalizationTable`: LocalizationTable
- [ ] `GuiBase2d.SelectionBehaviorDown`: SelectionBehavior
- [ ] `GuiBase2d.SelectionBehaviorLeft`: SelectionBehavior
- [ ] `GuiBase2d.SelectionBehaviorRight`: SelectionBehavior
- [ ] `GuiBase2d.SelectionBehaviorUp`: SelectionBehavior
- [ ] `GuiBase2d.SelectionChanged` (evento: amISelected: bool, previousSelection: GuiObject, newSelection: GuiObject)
- [ ] `GuiBase2d.SelectionGroup`: bool

### GuiObject (eredita da GuiBase2d) _NotCreatable_
- [ ] `GuiObject.Active`: bool
- [ ] `GuiObject.AnchorPoint`: Vector2
- [ ] `GuiObject.AutomaticSize`: AutomaticSize
- [ ] `GuiObject.BackgroundColor3`: Color3
- [ ] `GuiObject.BackgroundTransparency`: float
- [ ] `GuiObject.BorderColor3`: Color3
- [ ] `GuiObject.BorderMode`: BorderMode
- [ ] `GuiObject.BorderSizePixel`: int
- [ ] `GuiObject.ClipsDescendants`: bool
- [ ] `GuiObject.GuiState`: GuiState  _(ReadOnly)_
- [ ] `GuiObject.InputBegan` (evento: input: InputObject)
- [ ] `GuiObject.InputChanged` (evento: input: InputObject)
- [ ] `GuiObject.InputEnded` (evento: input: InputObject)
- [ ] `GuiObject.InputSink`: InputSink
- [ ] `GuiObject.Interactable`: bool
- [ ] `GuiObject.LayoutOrder`: int
- [ ] `GuiObject.MouseEnter` (evento: x: int, y: int)
- [ ] `GuiObject.MouseLeave` (evento: x: int, y: int)
- [ ] `GuiObject.MouseMoved` (evento: x: int, y: int)
- [ ] `GuiObject.MouseWheelBackward` (evento: x: int, y: int)
- [ ] `GuiObject.MouseWheelForward` (evento: x: int, y: int)
- [ ] `GuiObject.NextSelectionDown`: GuiObject
- [ ] `GuiObject.NextSelectionLeft`: GuiObject
- [ ] `GuiObject.NextSelectionRight`: GuiObject
- [ ] `GuiObject.NextSelectionUp`: GuiObject
- [ ] `GuiObject.Position`: UDim2
- [ ] `GuiObject.Rotation`: float
- [ ] `GuiObject.Selectable`: bool
- [ ] `GuiObject.SelectionGained` (evento: inputDirection: GuiNavigationDirection)
- [ ] `GuiObject.SelectionImageObject`: GuiObject
- [ ] `GuiObject.SelectionLost` (evento: inputDirection: GuiNavigationDirection)
- [ ] `GuiObject.SelectionOrder`: int
- [ ] `GuiObject.SizeConstraint`: SizeConstraint
- [ ] `GuiObject.Size`: UDim2
- [ ] `GuiObject.TouchLongPress` (evento: touchPositions: Array, state: UserInputState)
- [ ] `GuiObject.TouchPan` (evento: touchPositions: Array, totalTranslation: Vector2, velocity: Vector2, state: UserInputState)
- [ ] `GuiObject.TouchPinch` (evento: touchPositions: Array, scale: float, velocity: float, state: UserInputState)
- [ ] `GuiObject.TouchRotate` (evento: touchPositions: Array, rotation: float, velocity: float, state: UserInputState)
- [ ] `GuiObject.TouchSwipe` (evento: swipeDirection: SwipeDirection, numberOfTouches: int)
- [ ] `GuiObject.TouchTap` (evento: touchPositions: Array)
- [ ] `GuiObject.Visible`: bool
- [ ] `GuiObject.ZIndex`: int

### GuiButton (eredita da GuiObject) _NotCreatable_
- [ ] `GuiButton.Activated` (evento: inputObject: InputObject, clickCount: int)
- [ ] `GuiButton.AutoButtonColor`: bool
- [ ] `GuiButton.HoverHapticEffect`: HapticEffect
- [ ] `GuiButton.Modal`: bool
- [ ] `GuiButton.MouseButton1Click` (evento: nessun argomento)
- [ ] `GuiButton.MouseButton1Down` (evento: x: int, y: int)
- [ ] `GuiButton.MouseButton1Up` (evento: x: int, y: int)
- [ ] `GuiButton.MouseButton2Click` (evento: nessun argomento)
- [ ] `GuiButton.MouseButton2Down` (evento: x: int, y: int)
- [ ] `GuiButton.MouseButton2Up` (evento: x: int, y: int)
- [ ] `GuiButton.PressHapticEffect`: HapticEffect
- [ ] `GuiButton.SecondaryActivated` (evento: inputObject: InputObject)
- [ ] `GuiButton.Selected`: bool
- [ ] `GuiButton.Style`: ButtonStyle

### GuiLabel (eredita da GuiObject) _NotCreatable_
_(nessun membro proprio: vedi la classe da cui eredita)_

### Frame (eredita da GuiObject)
- [ ] `Frame.Style`: FrameStyle = Custom

### TextLabel (eredita da GuiLabel)
- [ ] `TextLabel.ContentText`: string = Label  _(ReadOnly)_
- [ ] `TextLabel.FontFace`: Font
- [ ] `TextLabel.LineHeight`: float = 1
- [ ] `TextLabel.MaxVisibleGraphemes`: int = -1
- [ ] `TextLabel.OpenTypeFeaturesError`: string =   _(ReadOnly)_
- [ ] `TextLabel.OpenTypeFeatures`: string = 
- [ ] `TextLabel.RichText`: bool = false
- [ ] `TextLabel.TextBounds`: Vector2 = 0, 0  _(ReadOnly)_
- [ ] `TextLabel.TextColor3`: Color3 = 0.105882, 0.164706, 0.207843
- [ ] `TextLabel.TextDirection`: TextDirection = Auto
- [ ] `TextLabel.TextFits`: bool = false  _(ReadOnly)_
- [ ] `TextLabel.TextScaled`: bool = false
- [ ] `TextLabel.TextSize`: float = 8
- [ ] `TextLabel.TextStrokeColor3`: Color3 = 0, 0, 0
- [ ] `TextLabel.TextStrokeTransparency`: float = 1
- [ ] `TextLabel.TextTransparency`: float = 0
- [ ] `TextLabel.TextTruncate`: TextTruncate = None
- [ ] `TextLabel.TextWrapped`: bool = false
- [ ] `TextLabel.TextXAlignment`: TextXAlignment = Center
- [ ] `TextLabel.TextYAlignment`: TextYAlignment = Center
- [ ] `TextLabel.Text`: string = Label

### TextButton (eredita da GuiButton)
- [ ] `TextButton.ContentText`: string = Button  _(ReadOnly)_
- [ ] `TextButton.FontFace`: Font
- [ ] `TextButton.LineHeight`: float = 1
- [ ] `TextButton.MaxVisibleGraphemes`: int = -1
- [ ] `TextButton.OpenTypeFeaturesError`: string =   _(ReadOnly)_
- [ ] `TextButton.OpenTypeFeatures`: string = 
- [ ] `TextButton.RichText`: bool = false
- [ ] `TextButton.TextBounds`: Vector2 = 0, 0  _(ReadOnly)_
- [ ] `TextButton.TextColor3`: Color3 = 0.105882, 0.164706, 0.207843
- [ ] `TextButton.TextDirection`: TextDirection = Auto
- [ ] `TextButton.TextFits`: bool = false  _(ReadOnly)_
- [ ] `TextButton.TextScaled`: bool = false
- [ ] `TextButton.TextSize`: float = 8
- [ ] `TextButton.TextStrokeColor3`: Color3 = 0, 0, 0
- [ ] `TextButton.TextStrokeTransparency`: float = 1
- [ ] `TextButton.TextTransparency`: float = 0
- [ ] `TextButton.TextTruncate`: TextTruncate = None
- [ ] `TextButton.TextWrapped`: bool = false
- [ ] `TextButton.TextXAlignment`: TextXAlignment = Center
- [ ] `TextButton.TextYAlignment`: TextYAlignment = Center
- [ ] `TextButton.Text`: string = Button

### TextBox (eredita da GuiObject)
- [ ] `TextBox.ClearTextOnFocus`: bool = true
- [ ] `TextBox.ContentText`: string = TextBox  _(ReadOnly)_
- [ ] `TextBox.CursorPosition`: int = 1
- [ ] `TextBox.FocusLost` (evento: enterPressed: bool, inputThatCausedFocusLoss: InputObject)
- [ ] `TextBox.Focused` (evento: nessun argomento)
- [ ] `TextBox.FontFace`: Font
- [ ] `TextBox.LineHeight`: float = 1
- [ ] `TextBox.MaxVisibleGraphemes`: int = -1
- [ ] `TextBox.MultiLine`: bool = false
- [ ] `TextBox.OpenTypeFeaturesError`: string =   _(ReadOnly)_
- [ ] `TextBox.OpenTypeFeatures`: string = 
- [ ] `TextBox.PlaceholderColor3`: Color3 = 0.5, 0.5, 0.5
- [ ] `TextBox.PlaceholderText`: string = 
- [ ] `TextBox.ReturnPressedFromOnScreenKeyboard` (evento: nessun argomento)
- [ ] `TextBox.RichText`: bool = false
- [ ] `TextBox.SelectionStart`: int = -1
- [ ] `TextBox.ShowNativeInput`: bool = true
- [ ] `TextBox.TextBounds`: Vector2 = 0, 0  _(ReadOnly)_
- [ ] `TextBox.TextColor3`: Color3 = 0.105882, 0.164706, 0.207843
- [ ] `TextBox.TextDirection`: TextDirection = Auto
- [ ] `TextBox.TextEditable`: bool = true
- [ ] `TextBox.TextFits`: bool = false  _(ReadOnly)_
- [ ] `TextBox.TextScaled`: bool = false
- [ ] `TextBox.TextSize`: float = 8
- [ ] `TextBox.TextStrokeColor3`: Color3 = 0, 0, 0
- [ ] `TextBox.TextStrokeTransparency`: float = 1
- [ ] `TextBox.TextTransparency`: float = 0
- [ ] `TextBox.TextTruncate`: TextTruncate = None
- [ ] `TextBox.TextWrapped`: bool = false
- [ ] `TextBox.TextXAlignment`: TextXAlignment = Center
- [ ] `TextBox.TextYAlignment`: TextYAlignment = Center
- [ ] `TextBox.Text`: string = TextBox
- [ ] `TextBox:CaptureFocus()` -> null
- [ ] `TextBox:IsFocused()` -> bool
- [ ] `TextBox:ReleaseFocus(submitted: bool = false)` -> null

### ImageLabel (eredita da GuiLabel)
- [ ] `ImageLabel.ImageColor3`: Color3 = 1, 1, 1
- [ ] `ImageLabel.ImageContent`: Content
- [ ] `ImageLabel.ImageRectOffset`: Vector2 = 0, 0
- [ ] `ImageLabel.ImageRectSize`: Vector2 = 0, 0
- [ ] `ImageLabel.ImageTransparency`: float = 0
- [ ] `ImageLabel.Image`: ContentId = 
- [ ] `ImageLabel.IsLoaded`: bool = false  _(ReadOnly)_
- [ ] `ImageLabel.ResampleMode`: ResamplerMode = Default
- [ ] `ImageLabel.ScaleType`: ScaleType = Stretch
- [ ] `ImageLabel.SliceCenter`: Rect = 0, 0, 0, 0
- [ ] `ImageLabel.SliceScale`: float = 1
- [ ] `ImageLabel.TileSize`: UDim2 = {1, 0}, {1, 0}

### ImageButton (eredita da GuiButton)
- [ ] `ImageButton.HoverImageContent`: Content
- [ ] `ImageButton.HoverImage`: ContentId = 
- [ ] `ImageButton.ImageColor3`: Color3 = 1, 1, 1
- [ ] `ImageButton.ImageContent`: Content
- [ ] `ImageButton.ImageRectOffset`: Vector2 = 0, 0
- [ ] `ImageButton.ImageRectSize`: Vector2 = 0, 0
- [ ] `ImageButton.ImageTransparency`: float = 0
- [ ] `ImageButton.Image`: ContentId = 
- [ ] `ImageButton.IsLoaded`: bool = false  _(ReadOnly)_
- [ ] `ImageButton.PressedImageContent`: Content
- [ ] `ImageButton.PressedImage`: ContentId = 
- [ ] `ImageButton.ResampleMode`: ResamplerMode = Default
- [ ] `ImageButton.ScaleType`: ScaleType = Stretch
- [ ] `ImageButton.SliceCenter`: Rect = 0, 0, 0, 0
- [ ] `ImageButton.SliceScale`: float = 1
- [ ] `ImageButton.TileSize`: UDim2 = {1, 0}, {1, 0}

### ScrollingFrame (eredita da GuiObject)
- [ ] `ScrollingFrame.AbsoluteCanvasSize`: Vector2 = 0, 0  _(ReadOnly)_
- [ ] `ScrollingFrame.AbsoluteWindowSize`: Vector2 = 0, 0  _(ReadOnly)_
- [ ] `ScrollingFrame.AutomaticCanvasSize`: AutomaticSize = None
- [ ] `ScrollingFrame.BottomImageContent`: Content
- [ ] `ScrollingFrame.BottomImage`: ContentId = rbxasset://textures/ui/Scroll/scroll-bottom.png
- [ ] `ScrollingFrame.CanvasPosition`: Vector2 = 0, 0
- [ ] `ScrollingFrame.CanvasSize`: UDim2 = {0, 0}, {2, 0}
- [ ] `ScrollingFrame.ElasticBehavior`: ElasticBehavior = WhenScrollable
- [ ] `ScrollingFrame.HorizontalScrollBarInset`: ScrollBarInset = None
- [ ] `ScrollingFrame.MidImageContent`: Content
- [ ] `ScrollingFrame.MidImage`: ContentId = rbxasset://textures/ui/Scroll/scroll-middle.png
- [ ] `ScrollingFrame.ScrollBarImageColor3`: Color3 = 1, 1, 1
- [ ] `ScrollingFrame.ScrollBarImageTransparency`: float = 0
- [ ] `ScrollingFrame.ScrollBarThickness`: int = 12
- [ ] `ScrollingFrame.ScrollingDirection`: ScrollingDirection = XY
- [ ] `ScrollingFrame.ScrollingEnabled`: bool = true
- [ ] `ScrollingFrame.TopImageContent`: Content
- [ ] `ScrollingFrame.TopImage`: ContentId = rbxasset://textures/ui/Scroll/scroll-top.png
- [ ] `ScrollingFrame.VerticalScrollBarInset`: ScrollBarInset = None
- [ ] `ScrollingFrame.VerticalScrollBarPosition`: VerticalScrollBarPosition = Right
- [ ] `ScrollingFrame:GetScrollVelocity()` -> Vector2
- [ ] `ScrollingFrame:ResetScrollVelocity()` -> null

### ViewportFrame (eredita da GuiObject)
- [ ] `ViewportFrame.Ambient`: Color3 = 0.784314, 0.784314, 0.784314
- [ ] `ViewportFrame.CurrentCamera`: Camera
- [ ] `ViewportFrame.ImageColor3`: Color3 = 1, 1, 1
- [ ] `ViewportFrame.ImageTransparency`: float = 0
- [ ] `ViewportFrame.LightColor`: Color3 = 0.54902, 0.54902, 0.54902
- [ ] `ViewportFrame.LightDirection`: Vector3 = -1, -1, -1

### WorldModel (eredita da WorldRoot)
- [ ] `WorldModel.UseWorkspaceCollisionGroups`: bool = false

### CanvasGroup (eredita da GuiObject)
- [ ] `CanvasGroup.GroupColor3`: Color3 = 1, 1, 1
- [ ] `CanvasGroup.GroupTransparency`: float = 0

### LayerCollector (eredita da GuiBase2d) _NotCreatable_
- [ ] `LayerCollector.Enabled`: bool
- [ ] `LayerCollector.InputSinkEnabled`: RolloutState
- [ ] `LayerCollector.ResetOnSpawn`: bool
- [ ] `LayerCollector.ZIndexBehavior`: ZIndexBehavior
- [ ] `LayerCollector:GetGuiObjectsAtPosition(x: int, y: int)` -> Instances

### ScreenGui (eredita da LayerCollector)
- [ ] `ScreenGui.ClipToDeviceSafeArea`: bool = true
- [ ] `ScreenGui.DisplayOrder`: int = 0
- [ ] `ScreenGui.IgnoreGuiInset`: bool = false
- [ ] `ScreenGui.SafeAreaCompatibility`: SafeAreaCompatibility = FullscreenExtension
- [ ] `ScreenGui.ScreenInsets`: ScreenInsets = CoreUISafeInsets

### BillboardGui (eredita da LayerCollector)
- [ ] `BillboardGui.Active`: bool = false
- [ ] `BillboardGui.Adornee`: Instance
- [ ] `BillboardGui.AlwaysOnTop`: bool = false
- [ ] `BillboardGui.Brightness`: float = 1
- [ ] `BillboardGui.ClipsDescendants`: bool = false
- [ ] `BillboardGui.CurrentDistance`: float = 0  _(ReadOnly)_
- [ ] `BillboardGui.DistanceStep`: float = 0
- [ ] `BillboardGui.ExtentsOffsetWorldSpace`: Vector3 = 0, 0, 0
- [ ] `BillboardGui.ExtentsOffset`: Vector3 = 0, 0, 0
- [ ] `BillboardGui.LightInfluence`: float = 0
- [ ] `BillboardGui.MaxDistance`: float = INF
- [ ] `BillboardGui.PlayerToHideFrom`: Instance
- [ ] `BillboardGui.SizeOffset`: Vector2 = 0, 0
- [ ] `BillboardGui.Size`: UDim2 = {0, 0}, {0, 0}
- [ ] `BillboardGui.StudsOffsetWorldSpace`: Vector3 = 0, 0, 0
- [ ] `BillboardGui.StudsOffset`: Vector3 = 0, 0, 0

### SurfaceGuiBase (eredita da LayerCollector) _NotCreatable_
- [ ] `SurfaceGuiBase.Active`: bool
- [ ] `SurfaceGuiBase.Adornee`: Instance
- [ ] `SurfaceGuiBase.Face`: NormalId

### SurfaceGui (eredita da SurfaceGuiBase)
- [ ] `SurfaceGui.AlwaysOnTop`: bool = false
- [ ] `SurfaceGui.Brightness`: float = 1
- [ ] `SurfaceGui.CanvasSize`: Vector2 = 800, 600
- [ ] `SurfaceGui.ClipsDescendants`: bool = false
- [ ] `SurfaceGui.LightInfluence`: float = 0
- [ ] `SurfaceGui.MaxDistance`: float = 0
- [ ] `SurfaceGui.PixelsPerStud`: float = 50
- [ ] `SurfaceGui.SizingMode`: SurfaceGuiSizingMode = FixedSize
- [ ] `SurfaceGui.ToolPunchThroughDistance`: float = 0
- [ ] `SurfaceGui.ZOffset`: float = 0

### PlayerGui (eredita da BasePlayerGui) _NotCreatable_
- [ ] `PlayerGui.CurrentScreenOrientation`: ScreenOrientation = LandscapeLeft  _(ReadOnly)_
- [ ] `PlayerGui.ScreenOrientation`: ScreenOrientation = LandscapeSensor
- [ ] `PlayerGui.SelectionImageObject`: GuiObject

### StarterGui (eredita da BasePlayerGui) _NotCreatable, Service_
- [ ] `StarterGui.ScreenOrientation`: ScreenOrientation
- [ ] `StarterGui.ShowDevelopmentGui`: bool
- [ ] `StarterGui:GetCore(parameterName: string)` -> Variant  _(Yields)_
- [ ] `StarterGui:GetCoreGuiEnabled(coreGuiType: CoreGuiType)` -> bool
- [ ] `StarterGui:SetCore(parameterName: string, value: Variant)` -> null
- [ ] `StarterGui:SetCoreGuiEnabled(coreGuiType: CoreGuiType, enabled: bool)` -> null

### GuiService (eredita da Instance) _NotCreatable, Service_
- [ ] `GuiService.AutoSelectGuiEnabled`: bool
- [ ] `GuiService.GuiNavigationEnabled`: bool
- [ ] `GuiService.MenuClosed` (evento: nessun argomento)
- [ ] `GuiService.MenuIsOpen`: bool  _(ReadOnly)_
- [ ] `GuiService.MenuOpened` (evento: nessun argomento)
- [ ] `GuiService.PreferredTextSize`: PreferredTextSize  _(ReadOnly)_
- [ ] `GuiService.SelectedObject`: GuiObject
- [ ] `GuiService.TopbarInset`: Rect  _(ReadOnly)_
- [ ] `GuiService.TouchControlsEnabled`: bool
- [ ] `GuiService.ViewportDisplaySize`: DisplaySize  _(ReadOnly)_
- [ ] `GuiService:CloseInspectMenu()` -> null
- [ ] `GuiService:DismissNotification(notificationId: string)` -> bool
- [ ] `GuiService:GetEmotesMenuOpen()` -> bool
- [ ] `GuiService:GetGameplayPausedNotificationEnabled()` -> bool
- [ ] `GuiService:GetGuiInset()` -> Tuple
- [ ] `GuiService:GetInsetArea(screenInsets: ScreenInsets)` -> Rect
- [ ] `GuiService:GetInspectMenuEnabled()` -> bool
- [ ] `GuiService:InspectPlayerFromHumanoidDescription(humanoidDescription: Instance, name: string)` -> null
- [ ] `GuiService:InspectPlayerFromUserId(userId: User)` -> null
- [ ] `GuiService:IsTenFootInterface()` -> bool
- [ ] `GuiService:Select(selectionParent: Instance)` -> null
- [ ] `GuiService:SendNotification(notificationInfo: Dictionary)` -> string
- [ ] `GuiService:SetEmotesMenuOpen(isOpen: bool)` -> null
- [ ] `GuiService:SetGameplayPausedNotificationEnabled(enabled: bool)` -> null
- [ ] `GuiService:SetInspectMenuEnabled(enabled: bool)` -> null

### TextService (eredita da Instance) _NotCreatable, Service_
- [ ] `TextService:FilterAndTranslateStringAsync(stringToFilter: string, fromUserId: int64, targetLocales: Array, textContext: TextFilterContext = PrivateChat)` -> TextFilterTranslatedResult  _(Yields)_
- [ ] `TextService:FilterStringAsync(stringToFilter: string, fromUserId: int64, textContext: TextFilterContext = PrivateChat)` -> TextFilterResult  _(Yields)_
- [ ] `TextService:GetFamilyInfoAsync(assetId: ContentId)` -> Dictionary  _(Yields)_
- [ ] `TextService:GetTextBoundsAsync(params: GetTextBoundsParams)` -> Vector2  _(Yields)_
- [ ] `TextService:GetTextSize(string: string, fontSize: int, font: Font, frameSize: Vector2)` -> Vector2
- [ ] `TextService:GetTextSizeOffsetAsync(fontSize: int, font: Font)` -> float  _(Yields)_

### UIComponent (eredita da UIBase) _NotCreatable_
_(nessun membro proprio: vedi la classe da cui eredita)_

### UIBase (eredita da Instance) _NotCreatable_
_(nessun membro proprio: vedi la classe da cui eredita)_

### UILayout (eredita da UIComponent) _NotCreatable_
_(nessun membro proprio: vedi la classe da cui eredita)_

### UIGridStyleLayout (eredita da UILayout) _NotCreatable_
- [ ] `UIGridStyleLayout.AbsoluteContentSize`: Vector2  _(ReadOnly)_
- [ ] `UIGridStyleLayout.FillDirection`: FillDirection
- [ ] `UIGridStyleLayout.HorizontalAlignment`: HorizontalAlignment
- [ ] `UIGridStyleLayout.SortOrder`: SortOrder
- [ ] `UIGridStyleLayout.VerticalAlignment`: VerticalAlignment

### UIListLayout (eredita da UIGridStyleLayout)
- [ ] `UIListLayout.HorizontalFlex`: UIFlexAlignment = None
- [ ] `UIListLayout.ItemLineAlignment`: ItemLineAlignment = Automatic
- [ ] `UIListLayout.Padding`: UDim = 0, 0
- [ ] `UIListLayout.VerticalFlex`: UIFlexAlignment = None
- [ ] `UIListLayout.Wraps`: bool = false

### UIGridLayout (eredita da UIGridStyleLayout)
- [ ] `UIGridLayout.AbsoluteCellCount`: Vector2 = 0, 0  _(ReadOnly)_
- [ ] `UIGridLayout.AbsoluteCellSize`: Vector2 = 100, 100  _(ReadOnly)_
- [ ] `UIGridLayout.CellPadding`: UDim2 = {0, 5}, {0, 5}
- [ ] `UIGridLayout.CellSize`: UDim2 = {0, 100}, {0, 100}
- [ ] `UIGridLayout.FillDirectionMaxCells`: int = 0
- [ ] `UIGridLayout.StartCorner`: StartCorner = TopLeft

### UIPageLayout (eredita da UIGridStyleLayout)
- [ ] `UIPageLayout.Animated`: bool = true
- [ ] `UIPageLayout.Circular`: bool = false
- [ ] `UIPageLayout.CurrentPage`: GuiObject  _(ReadOnly)_
- [ ] `UIPageLayout.EasingDirection`: EasingDirection = Out
- [ ] `UIPageLayout.EasingStyle`: EasingStyle = Back
- [ ] `UIPageLayout.GamepadInputEnabled`: bool = true
- [ ] `UIPageLayout.Padding`: UDim = 0, 0
- [ ] `UIPageLayout.PageEnter` (evento: page: Instance)
- [ ] `UIPageLayout.PageLeave` (evento: page: Instance)
- [ ] `UIPageLayout.ScrollWheelInputEnabled`: bool = true
- [ ] `UIPageLayout.Stopped` (evento: currentPage: Instance)
- [ ] `UIPageLayout.TouchInputEnabled`: bool = true
- [ ] `UIPageLayout.TweenTime`: float = 1
- [ ] `UIPageLayout:JumpTo(page: Instance)` -> null
- [ ] `UIPageLayout:JumpToIndex(index: int)` -> null
- [ ] `UIPageLayout:Next()` -> null
- [ ] `UIPageLayout:Previous()` -> null

### UITableLayout (eredita da UIGridStyleLayout)
- [ ] `UITableLayout.FillEmptySpaceColumns`: bool = false
- [ ] `UITableLayout.FillEmptySpaceRows`: bool = false
- [ ] `UITableLayout.MajorAxis`: TableMajorAxis = RowMajor
- [ ] `UITableLayout.Padding`: UDim2 = {0, 0}, {0, 0}

### UIPadding (eredita da UIComponent)
- [ ] `UIPadding.PaddingBottom`: UDim = 0, 0
- [ ] `UIPadding.PaddingLeft`: UDim = 0, 0
- [ ] `UIPadding.PaddingRight`: UDim = 0, 0
- [ ] `UIPadding.PaddingTop`: UDim = 0, 0

### UICorner (eredita da UIComponent)
- [ ] `UICorner.BottomLeftRadius`: UDim = 0, 8
- [ ] `UICorner.BottomRightRadius`: UDim = 0, 8
- [ ] `UICorner.CornerRadius`: UDim = 0, 8
- [ ] `UICorner.TopLeftRadius`: UDim = 0, 8
- [ ] `UICorner.TopRightRadius`: UDim = 0, 8

### UIStroke (eredita da UIComponent)
- [ ] `UIStroke.ApplyStrokeMode`: ApplyStrokeMode = Contextual
- [ ] `UIStroke.BorderOffset`: UDim = 0, 0
- [ ] `UIStroke.BorderStrokePosition`: BorderStrokePosition = Outer
- [ ] `UIStroke.Color`: Color3 = 0, 0, 0
- [ ] `UIStroke.Enabled`: bool = true
- [ ] `UIStroke.LineJoinMode`: LineJoinMode = Round
- [ ] `UIStroke.StrokeSizingMode`: StrokeSizingMode = FixedSize
- [ ] `UIStroke.Thickness`: float = 1
- [ ] `UIStroke.Transparency`: float = 0
- [ ] `UIStroke.ZIndex`: int = 1

### UIGradient (eredita da UIComponent)
- [ ] `UIGradient.Color`: ColorSequence = 0 1 1 1 0 1 1 1 1 0 
- [ ] `UIGradient.Enabled`: bool = true
- [ ] `UIGradient.Offset`: Vector2 = 0, 0
- [ ] `UIGradient.Rotation`: float = 0
- [ ] `UIGradient.Scale`: float = 1
- [ ] `UIGradient.TileMode`: GradientTileMode = Clamp
- [ ] `UIGradient.Transparency`: NumberSequence = 0 0 0 1 0 0 
- [ ] `UIGradient.Type`: GradientType = Linear

### UIScale (eredita da UIComponent)
- [ ] `UIScale.Scale`: float = 1

### UIConstraint (eredita da UIComponent) _NotCreatable_
_(nessun membro proprio: vedi la classe da cui eredita)_

### UIAspectRatioConstraint (eredita da UIConstraint)
- [ ] `UIAspectRatioConstraint.AspectRatio`: float = 1
- [ ] `UIAspectRatioConstraint.AspectType`: AspectType = FitWithinMaxSize
- [ ] `UIAspectRatioConstraint.DominantAxis`: DominantAxis = Width

### UISizeConstraint (eredita da UIConstraint)
- [ ] `UISizeConstraint.MaxSize`: Vector2 = INF, INF
- [ ] `UISizeConstraint.MinSize`: Vector2 = 0, 0

### UITextSizeConstraint (eredita da UIConstraint)
- [ ] `UITextSizeConstraint.MaxTextSize`: int = 100
- [ ] `UITextSizeConstraint.MinTextSize`: int = 1

### UIFlexItem (eredita da UIComponent)
- [ ] `UIFlexItem.FlexMode`: UIFlexMode = None
- [ ] `UIFlexItem.GrowRatio`: float = 0
- [ ] `UIFlexItem.ItemLineAlignment`: ItemLineAlignment = Automatic
- [ ] `UIFlexItem.ShrinkRatio`: float = 0

### UIDragDetector (eredita da UIComponent)
- [ ] `UIDragDetector.ActivatedCursorIconContent`: Content
- [ ] `UIDragDetector.ActivatedCursorIcon`: ContentId = 
- [ ] `UIDragDetector.BoundingBehavior`: UIDragDetectorBoundingBehavior = Automatic
- [ ] `UIDragDetector.BoundingUI`: GuiBase2d
- [ ] `UIDragDetector.CursorIconContent`: Content
- [ ] `UIDragDetector.CursorIcon`: ContentId = 
- [ ] `UIDragDetector.DragAxis`: Vector2 = 1, 0
- [ ] `UIDragDetector.DragContinue` (evento: inputPosition: Vector2)
- [ ] `UIDragDetector.DragEnd` (evento: inputPosition: Vector2)
- [ ] `UIDragDetector.DragRelativity`: UIDragDetectorDragRelativity = Absolute
- [ ] `UIDragDetector.DragRotation`: float = 0
- [ ] `UIDragDetector.DragSpace`: UIDragDetectorDragSpace = Parent
- [ ] `UIDragDetector.DragStart` (evento: inputPosition: Vector2)
- [ ] `UIDragDetector.DragStyle`: UIDragDetectorDragStyle = TranslatePlane
- [ ] `UIDragDetector.DragUDim2`: UDim2 = {0, 0}, {0, 0}
- [ ] `UIDragDetector.Enabled`: bool = true
- [ ] `UIDragDetector.MaxDragAngle`: float = 0
- [ ] `UIDragDetector.MaxDragTranslation`: UDim2 = {0, 0}, {0, 0}
- [ ] `UIDragDetector.MinDragAngle`: float = 0
- [ ] `UIDragDetector.MinDragTranslation`: UDim2 = {0, 0}, {0, 0}
- [ ] `UIDragDetector.ReferenceUIInstance`: GuiObject
- [ ] `UIDragDetector.ResponseStyle`: UIDragDetectorResponseStyle = Offset
- [ ] `UIDragDetector.Selectable`: bool = true
- [ ] `UIDragDetector.SelectionModeDragSpeed`: UDim2 = {0, 300}, {0, 300}
- [ ] `UIDragDetector.SelectionModeRotateSpeed`: float = 90
- [ ] `UIDragDetector.UIDragSpeedAxisMapping`: UIDragSpeedAxisMapping = XY
- [ ] `UIDragDetector:AddConstraintFunction(priority: int, function: Function)` -> RBXScriptConnection
- [ ] `UIDragDetector:GetReferencePosition()` -> UDim2
- [ ] `UIDragDetector:GetReferenceRotation()` -> float
- [ ] `UIDragDetector:SetDragStyleFunction(function: Function)` -> null

## P8 - Input, camera, tempo

### UserInputService (eredita da Instance) _NotCreatable, Service_
- [ ] `UserInputService.AccelerometerEnabled`: bool  _(ReadOnly)_
- [ ] `UserInputService.DeviceAccelerationChanged` (evento: acceleration: InputObject)
- [ ] `UserInputService.DeviceGravityChanged` (evento: gravity: InputObject)
- [ ] `UserInputService.DeviceRotationChanged` (evento: rotation: InputObject, cframe: CFrame)
- [ ] `UserInputService.GamepadConnected` (evento: gamepadNum: UserInputType)
- [ ] `UserInputService.GamepadDisconnected` (evento: gamepadNum: UserInputType)
- [ ] `UserInputService.GamepadEnabled`: bool  _(ReadOnly)_
- [ ] `UserInputService.GyroscopeEnabled`: bool  _(ReadOnly)_
- [ ] `UserInputService.InputBegan` (evento: input: InputObject, gameProcessedEvent: bool)
- [ ] `UserInputService.InputChanged` (evento: input: InputObject, gameProcessedEvent: bool)
- [ ] `UserInputService.InputEnded` (evento: input: InputObject, gameProcessedEvent: bool)
- [ ] `UserInputService.JumpRequest` (evento: nessun argomento)
- [ ] `UserInputService.KeyboardEnabled`: bool  _(ReadOnly)_
- [ ] `UserInputService.LastInputTypeChanged` (evento: lastInputType: UserInputType)
- [ ] `UserInputService.MouseBehavior`: MouseBehavior
- [ ] `UserInputService.MouseDeltaSensitivity`: float
- [ ] `UserInputService.MouseEnabled`: bool  _(ReadOnly)_
- [ ] `UserInputService.MouseIconContent`: Content
- [ ] `UserInputService.MouseIconEnabled`: bool
- [ ] `UserInputService.MouseIcon`: ContentId
- [ ] `UserInputService.OnScreenKeyboardPosition`: Vector2  _(ReadOnly)_
- [ ] `UserInputService.OnScreenKeyboardSize`: Vector2  _(ReadOnly)_
- [ ] `UserInputService.OnScreenKeyboardVisible`: bool  _(ReadOnly)_
- [ ] `UserInputService.PointerAction` (evento: wheel: float, pan: Vector2, pinch: float, gameProcessedEvent: bool)
- [ ] `UserInputService.PreferredInput`: PreferredInput  _(ReadOnly)_
- [ ] `UserInputService.TextBoxFocusReleased` (evento: textboxReleased: TextBox)
- [ ] `UserInputService.TextBoxFocused` (evento: textboxFocused: TextBox)
- [ ] `UserInputService.TouchDrag` (evento: dragDirection: SwipeDirection, numberOfTouches: int, gameProcessedEvent: bool)
- [ ] `UserInputService.TouchEnabled`: bool  _(ReadOnly)_
- [ ] `UserInputService.TouchEnded` (evento: touch: InputObject, gameProcessedEvent: bool)
- [ ] `UserInputService.TouchLongPress` (evento: touchPositions: Array, state: UserInputState, gameProcessedEvent: bool)
- [ ] `UserInputService.TouchMoved` (evento: touch: InputObject, gameProcessedEvent: bool)
- [ ] `UserInputService.TouchPan` (evento: touchPositions: Array, totalTranslation: Vector2, velocity: Vector2, state: UserInputState, gameProcessedEvent: bool)
- [ ] `UserInputService.TouchPinch` (evento: touchPositions: Array, scale: float, velocity: float, state: UserInputState, gameProcessedEvent: bool)
- [ ] `UserInputService.TouchRotate` (evento: touchPositions: Array, rotation: float, velocity: float, state: UserInputState, gameProcessedEvent: bool)
- [ ] `UserInputService.TouchStarted` (evento: touch: InputObject, gameProcessedEvent: bool)
- [ ] `UserInputService.TouchSwipe` (evento: swipeDirection: SwipeDirection, numberOfTouches: int, gameProcessedEvent: bool)
- [ ] `UserInputService.TouchTapInWorld` (evento: position: Vector2, processedByUI: bool)
- [ ] `UserInputService.TouchTap` (evento: touchPositions: Array, gameProcessedEvent: bool)
- [ ] `UserInputService.VREnabled`: bool  _(ReadOnly)_
- [ ] `UserInputService.WindowFocusReleased` (evento: nessun argomento)
- [ ] `UserInputService.WindowFocused` (evento: nessun argomento)
- [ ] `UserInputService:CreateVirtualInput(target: Instance = nil)` -> Object
- [ ] `UserInputService:GamepadSupports(gamepadNum: UserInputType, gamepadKeyCode: KeyCode)` -> bool
- [ ] `UserInputService:GetConnectedGamepads()` -> Array
- [ ] `UserInputService:GetDeviceAcceleration()` -> InputObject
- [ ] `UserInputService:GetDeviceGravity()` -> InputObject
- [ ] `UserInputService:GetDeviceRotation()` -> Tuple
- [ ] `UserInputService:GetFocusedTextBox()` -> TextBox
- [ ] `UserInputService:GetGamepadConnected(gamepadNum: UserInputType)` -> bool
- [ ] `UserInputService:GetGamepadState(gamepadNum: UserInputType)` -> Instances
- [ ] `UserInputService:GetImageForKeyCode(keyCode: KeyCode)` -> ContentId
- [ ] `UserInputService:GetKeysPressed()` -> Instances
- [ ] `UserInputService:GetLastInputType()` -> UserInputType
- [ ] `UserInputService:GetMouseButtonsPressed()` -> Instances
- [ ] `UserInputService:GetMouseDelta()` -> Vector2
- [ ] `UserInputService:GetMouseLocation()` -> Vector2
- [ ] `UserInputService:GetNavigationGamepads()` -> Array
- [ ] `UserInputService:GetStringForKeyCode(keyCode: KeyCode, format: KeyCodeStringFormat = Default)` -> string
- [ ] `UserInputService:GetSupportedGamepadKeyCodes(gamepadNum: UserInputType)` -> Array
- [ ] `UserInputService:IsGamepadButtonDown(gamepadNum: UserInputType, gamepadKeyCode: KeyCode)` -> bool
- [ ] `UserInputService:IsKeyDown(keyCode: KeyCode)` -> bool
- [ ] `UserInputService:IsMouseButtonPressed(mouseButton: UserInputType)` -> bool
- [ ] `UserInputService:IsNavigationGamepad(gamepadEnum: UserInputType)` -> bool
- [ ] `UserInputService:RecenterUserHeadCFrame()` -> null
- [ ] `UserInputService:SetNavigationGamepad(gamepadEnum: UserInputType, enabled: bool)` -> null

### InputObject (eredita da Instance) _NotCreatable_
- [ ] `InputObject.Delta`: Vector3 = 0, 0, 0
- [ ] `InputObject.KeyCode`: KeyCode = None
- [ ] `InputObject.Position`: Vector3 = 0, 0, 0
- [ ] `InputObject.UserInputState`: UserInputState = None
- [ ] `InputObject.UserInputType`: UserInputType = None
- [ ] `InputObject:IsModifierKeyDown(modifierKey: ModifierKey)` -> bool

### ContextActionService (eredita da Instance) _NotCreatable, Service_
- [ ] `ContextActionService.LocalToolEquipped` (evento: toolEquipped: Instance)
- [ ] `ContextActionService.LocalToolUnequipped` (evento: toolUnequipped: Instance)
- [ ] `ContextActionService:BindAction(actionName: string, functionToBind: Function, createTouchButton: bool, inputTypes: Tuple)` -> null
- [ ] `ContextActionService:BindActionAtPriority(actionName: string, functionToBind: Function, createTouchButton: bool, priorityLevel: int, inputTypes: Tuple)` -> null
- [ ] `ContextActionService:BindActivate(userInputTypeForActivation: UserInputType, keyCodesForActivation: Tuple)` -> null
- [ ] `ContextActionService:GetAllBoundActionInfo()` -> Dictionary
- [ ] `ContextActionService:GetBoundActionInfo(actionName: string)` -> Dictionary
- [ ] `ContextActionService:GetButton(actionName: string)` -> Instance  _(Yields)_
- [ ] `ContextActionService:GetCurrentLocalToolIcon()` -> string
- [ ] `ContextActionService:SetDescription(actionName: string, description: string)` -> null
- [ ] `ContextActionService:SetImage(actionName: string, image: string)` -> null
- [ ] `ContextActionService:SetPosition(actionName: string, position: UDim2)` -> null
- [ ] `ContextActionService:SetTitle(actionName: string, title: string)` -> null
- [ ] `ContextActionService:UnbindAction(actionName: string)` -> null
- [ ] `ContextActionService:UnbindActivate(userInputTypeForActivation: UserInputType, keyCodeForActivation: KeyCode = None)` -> null
- [ ] `ContextActionService:UnbindAllActions()` -> null

### Camera (eredita da PVInstance)
- [ ] `Camera.CFrame`: CFrame = 0, 20, 20, 1, 0, -0, -0, 0.707106829, 0.707106829, 0, -0.707106829, 0.707106829
- [ ] `Camera.CameraSubject`: Instance
- [ ] `Camera.CameraType`: CameraType = Fixed
- [ ] `Camera.DiagonalFieldOfView`: float = 88.8765335
- [ ] `Camera.FieldOfViewMode`: FieldOfViewMode = Vertical
- [ ] `Camera.FieldOfView`: float = 70
- [ ] `Camera.Focus`: CFrame = 0, 0, -5, 1, 0, 0, 0, 1, 0, 0, 0, 1
- [ ] `Camera.HeadLocked`: bool = true
- [ ] `Camera.HeadScale`: float = 1
- [ ] `Camera.InterpolationFinished` (evento: nessun argomento)
- [ ] `Camera.MaxAxisFieldOfView`: float = 70
- [ ] `Camera.NearPlaneZ`: float = -0.5  _(ReadOnly)_
- [ ] `Camera.OrthographicMode`: OrthographicMode = Vertical
- [ ] `Camera.OrthographicSize`: float = 35
- [ ] `Camera.ProjectionType`: ProjectionType = Perspective
- [ ] `Camera.VRTiltAndRollEnabled`: bool = false
- [ ] `Camera.ViewStretch`: float = 1
- [ ] `Camera.ViewportSize`: Vector2 = 1, 1  _(ReadOnly)_
- [ ] `Camera:GetPartsObscuringTarget(castPoints: Array, ignoreList: Instances)` -> Instances
- [ ] `Camera:GetRenderCFrame()` -> CFrame
- [ ] `Camera:GetRoll()` -> float
- [ ] `Camera:ScreenPointToRay(x: float, y: float, depth: float = 0)` -> Ray
- [ ] `Camera:SetRoll(rollAngle: float)` -> null
- [ ] `Camera:ViewportPointToRay(x: float, y: float, depth: float = 0)` -> Ray
- [ ] `Camera:WorldToScreenPoint(worldPoint: Vector3)` -> Tuple
- [ ] `Camera:WorldToViewportPoint(worldPoint: Vector3)` -> Tuple
- [ ] `Camera:ZoomToExtents(boundingBoxCFrame: CFrame, boundingBoxSize: Vector3)` -> null

### RunService (eredita da Instance) _NotCreatable, Service_
- [ ] `RunService.FrameNumber`: int64  _(ReadOnly)_
- [ ] `RunService.Heartbeat` (evento: deltaTime: double)
- [ ] `RunService.Misprediction` (evento: time: double, instances: Array, stats: Dictionary)
- [ ] `RunService.PostSimulation` (evento: deltaTimeSim: double)
- [ ] `RunService.PreAnimation` (evento: deltaTimeSim: double)
- [ ] `RunService.PreRender` (evento: deltaTimeRender: double)
- [ ] `RunService.PreSimulation` (evento: deltaTimeSim: double)
- [ ] `RunService.RenderStepped` (evento: deltaTime: double)
- [ ] `RunService.Rollback` (evento: time: double)
- [ ] `RunService.RunState`: RunState
- [ ] `RunService.Stepped` (evento: time: double, deltaTime: double)
- [ ] `RunService:BindToAnimation(function: Function, frequency: StepFrequency = Hz30, priority: int = 2000)` -> RBXScriptConnection
- [ ] `RunService:BindToAnimationParallel(function: Function, frequency: StepFrequency = Hz30, priority: int = 2000)` -> RBXScriptConnection
- [ ] `RunService:BindToRenderStep(name: string, priority: int, function: Function)` -> null
- [ ] `RunService:BindToSimulation(function: Function, frequency: StepFrequency = Hz30, priority: int = 2000)` -> RBXScriptConnection
- [ ] `RunService:BindToSimulationParallel(function: Function, frequency: StepFrequency = Hz30, priority: int = 2000)` -> RBXScriptConnection
- [ ] `RunService:GetPredictionStatus(context: Instance)` -> PredictionStatus
- [ ] `RunService:IsClient()` -> bool
- [ ] `RunService:IsEdit()` -> bool
- [ ] `RunService:IsResimulating()` -> bool
- [ ] `RunService:IsRunMode()` -> bool
- [ ] `RunService:IsRunning()` -> bool
- [ ] `RunService:IsServer()` -> bool
- [ ] `RunService:IsStudio()` -> bool
- [ ] `RunService:SetPredictionMode(context: Instance, mode: PredictionMode)` -> null
- [ ] `RunService:UnbindFromRenderStep(name: string)` -> null

### TweenBase (eredita da Instance) _NotCreatable_
- [ ] `TweenBase.Completed` (evento: playbackState: PlaybackState)
- [ ] `TweenBase.PlaybackState`: PlaybackState  _(ReadOnly)_
- [ ] `TweenBase:Cancel()` -> null
- [ ] `TweenBase:Pause()` -> null
- [ ] `TweenBase:Play()` -> null

### Tween (eredita da TweenBase)
- [ ] `Tween.Instance`: Instance  _(ReadOnly)_
- [ ] `Tween.TweenInfo`: TweenInfo = Time:1 DelayTime:0 RepeatCount:0 Reverses:False EasingDirection:Out EasingStyle:Quad  _(ReadOnly)_

### TweenService (eredita da Instance) _NotCreatable, Service_
- [ ] `TweenService:Create(instance: Instance, tweenInfo: TweenInfo, propertyTable: Dictionary)` -> Tween
- [ ] `TweenService:GetValue(alpha: float, easingStyle: EasingStyle, easingDirection: EasingDirection)` -> float
- [ ] `TweenService:SmoothDamp(current: Variant, target: Variant, velocity: Variant, smoothTime: float, maxSpeed: float?, dt: float?)` -> undefined

## P9 - Effetti, luci, atmosfera, suono

### ParticleEmitter (eredita da Instance)
- [ ] `ParticleEmitter.Acceleration`: Vector3 = 0, 0, 0
- [ ] `ParticleEmitter.Brightness`: float = 1
- [ ] `ParticleEmitter.Color`: ColorSequence = 0 1 1 1 0 1 1 1 1 0 
- [ ] `ParticleEmitter.Drag`: float = 0
- [ ] `ParticleEmitter.EmissionDirection`: NormalId = Top
- [ ] `ParticleEmitter.Enabled`: bool = true
- [ ] `ParticleEmitter.FlipbookBlendFrames`: bool = true
- [ ] `ParticleEmitter.FlipbookFramerate`: NumberRange = 1 1 
- [ ] `ParticleEmitter.FlipbookIncompatible`: string = Particle texture must be 1024 by 1024 to use flipbooks.
- [ ] `ParticleEmitter.FlipbookLayout`: ParticleFlipbookLayout = None
- [ ] `ParticleEmitter.FlipbookMode`: ParticleFlipbookMode = Loop
- [ ] `ParticleEmitter.FlipbookSizeX`: int = 1
- [ ] `ParticleEmitter.FlipbookSizeY`: int = 1
- [ ] `ParticleEmitter.FlipbookStartRandom`: bool = false
- [ ] `ParticleEmitter.Lifetime`: NumberRange = 5 10 
- [ ] `ParticleEmitter.LightEmission`: float = 0
- [ ] `ParticleEmitter.LightInfluence`: float = 0
- [ ] `ParticleEmitter.LockedToPart`: bool = false
- [ ] `ParticleEmitter.Orientation`: ParticleOrientation = FacingCamera
- [ ] `ParticleEmitter.Rate`: float = 20
- [ ] `ParticleEmitter.RotSpeed`: NumberRange = 0 0 
- [ ] `ParticleEmitter.Rotation`: NumberRange = 0 0 
- [ ] `ParticleEmitter.ShapeInOut`: ParticleEmitterShapeInOut = Outward
- [ ] `ParticleEmitter.ShapePartial`: float = 1
- [ ] `ParticleEmitter.ShapeStyle`: ParticleEmitterShapeStyle = Volume
- [ ] `ParticleEmitter.Shape`: ParticleEmitterShape = Box
- [ ] `ParticleEmitter.Size`: NumberSequence = 0 1 0 1 1 0 
- [ ] `ParticleEmitter.Speed`: NumberRange = 5 5 
- [ ] `ParticleEmitter.SpreadAngle`: Vector2 = 0, 0
- [ ] `ParticleEmitter.Squash`: NumberSequence = 0 0 0 1 0 0 
- [ ] `ParticleEmitter.TextureContent`: Content
- [ ] `ParticleEmitter.Texture`: ContentId = rbxasset://textures/particles/sparkles_main.dds
- [ ] `ParticleEmitter.TimeScale`: float = 1
- [ ] `ParticleEmitter.Transparency`: NumberSequence = 0 0 0 1 0 0 
- [ ] `ParticleEmitter.VelocityInheritance`: float = 0
- [ ] `ParticleEmitter.WindAffectsDrag`: bool = false
- [ ] `ParticleEmitter.ZOffset`: float = 0
- [ ] `ParticleEmitter:Clear()` -> null
- [ ] `ParticleEmitter:Emit(particleCount: int = 16)` -> null

### Beam (eredita da Instance)
- [ ] `Beam.Attachment0`: Attachment
- [ ] `Beam.Attachment1`: Attachment
- [ ] `Beam.Brightness`: float = 1
- [ ] `Beam.Color`: ColorSequence = 0 1 1 1 0 1 1 1 1 0 
- [ ] `Beam.CurveSize0`: float = 0
- [ ] `Beam.CurveSize1`: float = 0
- [ ] `Beam.Enabled`: bool = true
- [ ] `Beam.FaceCamera`: bool = false
- [ ] `Beam.LightEmission`: float = 0
- [ ] `Beam.LightInfluence`: float = 0
- [ ] `Beam.Segments`: int = 10
- [ ] `Beam.TextureContent`: Content
- [ ] `Beam.TextureLength`: float = 1
- [ ] `Beam.TextureMode`: TextureMode = Stretch
- [ ] `Beam.TextureSpeed`: float = 1
- [ ] `Beam.Texture`: ContentId = 
- [ ] `Beam.Transparency`: NumberSequence = 0 0.5 0 1 0.5 0 
- [ ] `Beam.Width0`: float = 1
- [ ] `Beam.Width1`: float = 1
- [ ] `Beam.ZOffset`: float = 0
- [ ] `Beam:SetTextureOffset(offset: float = 0)` -> null

### Trail (eredita da Instance)
- [ ] `Trail.Attachment0`: Attachment
- [ ] `Trail.Attachment1`: Attachment
- [ ] `Trail.Brightness`: float = 1
- [ ] `Trail.Color`: ColorSequence = 0 1 1 1 0 1 1 1 1 0 
- [ ] `Trail.Enabled`: bool = true
- [ ] `Trail.FaceCamera`: bool = false
- [ ] `Trail.Lifetime`: float = 2
- [ ] `Trail.LightEmission`: float = 0
- [ ] `Trail.LightInfluence`: float = 0
- [ ] `Trail.MaxLength`: float = 0
- [ ] `Trail.MinLength`: float = 0.100000001
- [ ] `Trail.TextureContent`: Content
- [ ] `Trail.TextureLength`: float = 1
- [ ] `Trail.TextureMode`: TextureMode = Stretch
- [ ] `Trail.Texture`: ContentId = 
- [ ] `Trail.Transparency`: NumberSequence = 0 0.5 0 1 0.5 0 
- [ ] `Trail.WidthScale`: NumberSequence = 0 1 0 1 1 0 
- [ ] `Trail:Clear()` -> null

### Fire (eredita da Instance)
- [ ] `Fire.Color`: Color3 = 0.92549, 0.545098, 0.27451
- [ ] `Fire.Enabled`: bool = true
- [ ] `Fire.Heat`: float = 9
- [ ] `Fire.SecondaryColor`: Color3 = 0.545098, 0.313726, 0.215686
- [ ] `Fire.Size`: float = 5
- [ ] `Fire.TimeScale`: float = 1

### Smoke (eredita da Instance)
- [ ] `Smoke.Color`: Color3 = 1, 1, 1
- [ ] `Smoke.Enabled`: bool = true
- [ ] `Smoke.Opacity`: float = 0.5
- [ ] `Smoke.RiseVelocity`: float = 1
- [ ] `Smoke.Size`: float = 1
- [ ] `Smoke.TimeScale`: float = 1

### Sparkles (eredita da Instance)
- [ ] `Sparkles.Enabled`: bool = true
- [ ] `Sparkles.SparkleColor`: Color3 = 0.564706, 0.0980392, 1
- [ ] `Sparkles.TimeScale`: float = 1

### Highlight (eredita da Instance)
- [ ] `Highlight.Adornee`: Instance
- [ ] `Highlight.DepthMode`: HighlightDepthMode = AlwaysOnTop
- [ ] `Highlight.Enabled`: bool = true
- [ ] `Highlight.FillColor`: Color3 = 1, 0, 0
- [ ] `Highlight.FillTransparency`: float = 0.5
- [ ] `Highlight.OutlineColor`: Color3 = 1, 1, 1
- [ ] `Highlight.OutlineTransparency`: float = 0

### PVAdornment (eredita da GuiBase3d) _NotCreatable_
- [ ] `PVAdornment.Adornee`: PVInstance

### SelectionBox (eredita da InstanceAdornment)
- [ ] `SelectionBox.LineThickness`: float = 0.150000006
- [ ] `SelectionBox.SurfaceColor3`: Color3 = 0.0509804, 0.411765, 0.67451
- [ ] `SelectionBox.SurfaceTransparency`: float = 1

### HandleAdornment (eredita da PVAdornment) _NotCreatable_
- [ ] `HandleAdornment.AdornCullingMode`: AdornCullingMode
- [ ] `HandleAdornment.AlwaysOnTop`: bool
- [ ] `HandleAdornment.CFrame`: CFrame
- [ ] `HandleAdornment.MouseButton1Down` (evento: nessun argomento)
- [ ] `HandleAdornment.MouseButton1Up` (evento: nessun argomento)
- [ ] `HandleAdornment.MouseEnter` (evento: nessun argomento)
- [ ] `HandleAdornment.MouseLeave` (evento: nessun argomento)
- [ ] `HandleAdornment.SizeRelativeOffset`: Vector3
- [ ] `HandleAdornment.ZIndex`: int

### BoxHandleAdornment (eredita da HandleAdornment)
- [ ] `BoxHandleAdornment.Shading`: AdornShading = Default
- [ ] `BoxHandleAdornment.Size`: Vector3 = 1, 1, 1

### SphereHandleAdornment (eredita da HandleAdornment)
- [ ] `SphereHandleAdornment.Radius`: float = 1
- [ ] `SphereHandleAdornment.Shading`: AdornShading = Default

### CylinderHandleAdornment (eredita da HandleAdornment)
- [ ] `CylinderHandleAdornment.Angle`: float = 360
- [ ] `CylinderHandleAdornment.Height`: float = 1
- [ ] `CylinderHandleAdornment.InnerRadius`: float = 0
- [ ] `CylinderHandleAdornment.Radius`: float = 1
- [ ] `CylinderHandleAdornment.Shading`: AdornShading = Default

### ConeHandleAdornment (eredita da HandleAdornment)
- [ ] `ConeHandleAdornment.Height`: float = 2
- [ ] `ConeHandleAdornment.Hollow`: bool = false
- [ ] `ConeHandleAdornment.Radius`: float = 0.5
- [ ] `ConeHandleAdornment.Shading`: AdornShading = Default

### LineHandleAdornment (eredita da HandleAdornment)
- [ ] `LineHandleAdornment.Length`: float = 5
- [ ] `LineHandleAdornment.Thickness`: float = 1

### Light (eredita da Instance) _NotCreatable_
- [ ] `Light.Brightness`: float
- [ ] `Light.Color`: Color3
- [ ] `Light.Enabled`: bool
- [ ] `Light.Shadows`: bool

### PointLight (eredita da Light)
- [ ] `PointLight.Range`: float

### SpotLight (eredita da Light)
- [ ] `SpotLight.Angle`: float
- [ ] `SpotLight.Face`: NormalId
- [ ] `SpotLight.Range`: float

### SurfaceLight (eredita da Light)
- [ ] `SurfaceLight.Angle`: float
- [ ] `SurfaceLight.Face`: NormalId
- [ ] `SurfaceLight.Range`: float

### Lighting (eredita da Instance) _NotCreatable, Service_
- [ ] `Lighting.Ambient`: Color3 = 0.5, 0.5, 0.5
- [ ] `Lighting.Brightness`: float = 1
- [ ] `Lighting.ClockTime`: float = 14
- [ ] `Lighting.ColorShift_Bottom`: Color3 = 0, 0, 0
- [ ] `Lighting.ColorShift_Top`: Color3 = 0, 0, 0
- [ ] `Lighting.EnvironmentDiffuseScale`: float = 0
- [ ] `Lighting.EnvironmentSpecularScale`: float = 0
- [ ] `Lighting.ExposureCompensation`: float = 0
- [ ] `Lighting.FogColor`: Color3 = 0.75, 0.75, 0.75
- [ ] `Lighting.FogEnd`: float = 100000
- [ ] `Lighting.FogStart`: float = 0
- [ ] `Lighting.GeographicLatitude`: float = 41.7332993
- [ ] `Lighting.GlobalShadows`: bool = false
- [ ] `Lighting.LightingChanged` (evento: skyChanged: bool)
- [ ] `Lighting.LightingStyle`: LightingStyle = Realistic
- [ ] `Lighting.OutdoorAmbient`: Color3 = 0.5, 0.5, 0.5
- [ ] `Lighting.PrioritizeLightingQuality`: bool = true
- [ ] `Lighting.ShadowSoftness`: float = 0.5
- [ ] `Lighting.TimeOfDay`: string = 14:00:00
- [ ] `Lighting:GetMinutesAfterMidnight()` -> double
- [ ] `Lighting:GetMoonDirection()` -> Vector3
- [ ] `Lighting:GetMoonPhase()` -> float
- [ ] `Lighting:GetSunDirection()` -> Vector3
- [ ] `Lighting:SetMinutesAfterMidnight(minutes: double)` -> null

### Sky (eredita da Instance)
- [ ] `Sky.CelestialBodiesShown`: bool = true
- [ ] `Sky.MoonAngularSize`: float = 11
- [ ] `Sky.MoonTextureContent`: Content
- [ ] `Sky.MoonTextureId`: ContentId = rbxasset://sky/moon.jpg
- [ ] `Sky.SkyboxBackContent`: Content
- [ ] `Sky.SkyboxBk`: ContentId = rbxasset://textures/sky/sky512_bk.tex
- [ ] `Sky.SkyboxDn`: ContentId = rbxasset://textures/sky/sky512_dn.tex
- [ ] `Sky.SkyboxDownContent`: Content
- [ ] `Sky.SkyboxFrontContent`: Content
- [ ] `Sky.SkyboxFt`: ContentId = rbxasset://textures/sky/sky512_ft.tex
- [ ] `Sky.SkyboxLeftContent`: Content
- [ ] `Sky.SkyboxLf`: ContentId = rbxasset://textures/sky/sky512_lf.tex
- [ ] `Sky.SkyboxOrientation`: Vector3 = 0, 0, 0
- [ ] `Sky.SkyboxRightContent`: Content
- [ ] `Sky.SkyboxRt`: ContentId = rbxasset://textures/sky/sky512_rt.tex
- [ ] `Sky.SkyboxUpContent`: Content
- [ ] `Sky.SkyboxUp`: ContentId = rbxasset://textures/sky/sky512_up.tex
- [ ] `Sky.StarCount`: int = 3000
- [ ] `Sky.SunAngularSize`: float = 21
- [ ] `Sky.SunTextureContent`: Content
- [ ] `Sky.SunTextureId`: ContentId = rbxasset://sky/sun.jpg

### Atmosphere (eredita da Instance)
- [ ] `Atmosphere.Color`: Color3 = 0.7843, 0.6667, 0.4235
- [ ] `Atmosphere.Decay`: Color3 = 0.3608, 0.2353, 0.0549
- [ ] `Atmosphere.Density`: float = 0.395000011
- [ ] `Atmosphere.Glare`: float = 0
- [ ] `Atmosphere.Haze`: float = 0
- [ ] `Atmosphere.Offset`: float = 0

### Clouds (eredita da Instance)
- [ ] `Clouds.Color`: Color3 = 1, 1, 1
- [ ] `Clouds.Cover`: float = 0.5
- [ ] `Clouds.Density`: float = 0.699999988
- [ ] `Clouds.Enabled`: bool = true

### PostEffect (eredita da Instance) _NotCreatable_
- [ ] `PostEffect.Enabled`: bool

### BloomEffect (eredita da PostEffect)
- [ ] `BloomEffect.Intensity`: float = 0.400000006
- [ ] `BloomEffect.Size`: float = 24
- [ ] `BloomEffect.Threshold`: float = 0.949999988

### BlurEffect (eredita da PostEffect)
- [ ] `BlurEffect.Size`: float = 24

### ColorCorrectionEffect (eredita da PostEffect)
- [ ] `ColorCorrectionEffect.Brightness`: float = 0
- [ ] `ColorCorrectionEffect.Contrast`: float = 0
- [ ] `ColorCorrectionEffect.Saturation`: float = 0
- [ ] `ColorCorrectionEffect.TintColor`: Color3 = 1, 1, 1

### SunRaysEffect (eredita da PostEffect)
- [ ] `SunRaysEffect.Intensity`: float = 0.25
- [ ] `SunRaysEffect.Spread`: float = 1

### DepthOfFieldEffect (eredita da PostEffect)
- [ ] `DepthOfFieldEffect.FarIntensity`: float = 0.75
- [ ] `DepthOfFieldEffect.FocusDistance`: float = 0.0500000007
- [ ] `DepthOfFieldEffect.InFocusRadius`: float = 10
- [ ] `DepthOfFieldEffect.NearIntensity`: float = 0.75

### Sound (eredita da Instance)
- [ ] `Sound.AcousticSimulationEnabled`: bool = true
- [ ] `Sound.AudioContent`: Content
- [ ] `Sound.DidLoop` (evento: soundId: string, numOfTimesLooped: int)
- [ ] `Sound.Ended` (evento: soundId: string)
- [ ] `Sound.IsLoaded`: bool = false  _(ReadOnly)_
- [ ] `Sound.Loaded` (evento: soundId: string)
- [ ] `Sound.LoopRegion`: NumberRange = 0 60000 
- [ ] `Sound.Looped`: bool = false
- [ ] `Sound.Paused` (evento: soundId: string)
- [ ] `Sound.PlayOnRemove`: bool = false
- [ ] `Sound.PlaybackLoudness`: double = 0  _(ReadOnly)_
- [ ] `Sound.PlaybackRegion`: NumberRange = 0 60000 
- [ ] `Sound.PlaybackRegionsEnabled`: bool = false
- [ ] `Sound.PlaybackSpeed`: float = 1
- [ ] `Sound.Played` (evento: soundId: string)
- [ ] `Sound.Playing`: bool = false
- [ ] `Sound.Resumed` (evento: soundId: string)
- [ ] `Sound.RollOffMaxDistance`: float = 10000
- [ ] `Sound.RollOffMinDistance`: float = 10
- [ ] `Sound.RollOffMode`: RollOffMode = Inverse
- [ ] `Sound.SoundGroup`: SoundGroup
- [ ] `Sound.SoundId`: ContentId = 
- [ ] `Sound.Stopped` (evento: soundId: string)
- [ ] `Sound.TimeLength`: double = 0  _(ReadOnly)_
- [ ] `Sound.TimePosition`: double = 0
- [ ] `Sound.Volume`: float = 0.5
- [ ] `Sound:Pause()` -> null
- [ ] `Sound:Play()` -> null
- [ ] `Sound:Resume()` -> null
- [ ] `Sound:Stop()` -> null

### SoundGroup (eredita da Instance)
- [ ] `SoundGroup.Volume`: float = 0.5

### SoundService (eredita da Instance) _NotCreatable, Service_
- [ ] `SoundService.AcousticSimulationEnabled`: bool = false
- [ ] `SoundService.AmbientReverb`: ReverbType = NoReverb
- [ ] `SoundService.CharacterSoundsUseNewApi`: RolloutState = Default
- [ ] `SoundService.DefaultListenerLocation`: ListenerLocation = Default
- [ ] `SoundService.DiffractionEnabled`: bool = true
- [ ] `SoundService.DistanceFactor`: float = 3.32999992
- [ ] `SoundService.DopplerScale`: float = 1
- [ ] `SoundService.ListenerCFrame`: CFrame = 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1
- [ ] `SoundService.ListenerObject`: Instance
- [ ] `SoundService.ListenerType`: ListenerType = Camera
- [ ] `SoundService.OcclusionEnabled`: bool = true
- [ ] `SoundService.RespectFilteringEnabled`: bool = false
- [ ] `SoundService.ReverbEnabled`: bool = true
- [ ] `SoundService.RolloffScale`: float = 1
- [ ] `SoundService:GetInputDevice()` -> Tuple
- [ ] `SoundService:GetListener()` -> Tuple
- [ ] `SoundService:GetMixerTime()` -> double
- [ ] `SoundService:PlayLocalSound(sound: Instance)` -> null
- [ ] `SoundService:SetInputDevice(nameOrInstance: Variant, guidOrPin: string)` -> null
- [ ] `SoundService:SetListener(listenerType: ListenerType, listener: Tuple)` -> null

### SoundEffect (eredita da Instance) _NotCreatable_
- [ ] `SoundEffect.Enabled`: bool
- [ ] `SoundEffect.Priority`: int

### ReverbSoundEffect (eredita da SoundEffect)
- [ ] `ReverbSoundEffect.DecayTime`: float = 1.5
- [ ] `ReverbSoundEffect.Density`: float = 1
- [ ] `ReverbSoundEffect.Diffusion`: float = 1
- [ ] `ReverbSoundEffect.DryLevel`: float = -6
- [ ] `ReverbSoundEffect.WetLevel`: float = 0

### EqualizerSoundEffect (eredita da SoundEffect)
- [ ] `EqualizerSoundEffect.HighGain`: float = 0
- [ ] `EqualizerSoundEffect.LowGain`: float = -20
- [ ] `EqualizerSoundEffect.MidGain`: float = -10

### EchoSoundEffect (eredita da SoundEffect)
- [ ] `EchoSoundEffect.Delay`: float = 1
- [ ] `EchoSoundEffect.DryLevel`: float = 0
- [ ] `EchoSoundEffect.Feedback`: float = 0.5
- [ ] `EchoSoundEffect.WetLevel`: float = 0

### DistortionSoundEffect (eredita da SoundEffect)
- [ ] `DistortionSoundEffect.Level`: float = 0.75

### ChorusSoundEffect (eredita da SoundEffect)
- [ ] `ChorusSoundEffect.Depth`: float = 0.150000006
- [ ] `ChorusSoundEffect.Mix`: float = 0.5
- [ ] `ChorusSoundEffect.Rate`: float = 0.5

### CompressorSoundEffect (eredita da SoundEffect)
- [ ] `CompressorSoundEffect.Attack`: float = 0.100000001
- [ ] `CompressorSoundEffect.GainMakeup`: float = 0
- [ ] `CompressorSoundEffect.Ratio`: float = 40
- [ ] `CompressorSoundEffect.Release`: float = 0.100000001
- [ ] `CompressorSoundEffect.SideChain`: Instance
- [ ] `CompressorSoundEffect.Threshold`: float = -40

### FlangeSoundEffect (eredita da SoundEffect)
- [ ] `FlangeSoundEffect.Depth`: float = 0.449999988
- [ ] `FlangeSoundEffect.Mix`: float = 0.850000024
- [ ] `FlangeSoundEffect.Rate`: float = 5

### PitchShiftSoundEffect (eredita da SoundEffect)
- [ ] `PitchShiftSoundEffect.Octave`: float = 1.25

### TremoloSoundEffect (eredita da SoundEffect)
- [ ] `TremoloSoundEffect.Depth`: float = 1
- [ ] `TremoloSoundEffect.Duty`: float = 0.5
- [ ] `TremoloSoundEffect.Frequency`: float = 5

## P10 - Dati, rete e servizi

### DataModel (eredita da ServiceProvider) _NotCreatable_
- [ ] `DataModel.CreatorId`: int64  _(ReadOnly)_
- [ ] `DataModel.CreatorType`: CreatorType  _(ReadOnly)_
- [ ] `DataModel.GameId`: int64  _(ReadOnly)_
- [ ] `DataModel.Genre`: Genre  _(ReadOnly)_
- [ ] `DataModel.GraphicsQualityChangeRequest` (evento: betterQuality: bool)
- [ ] `DataModel.JobId`: string  _(ReadOnly)_
- [ ] `DataModel.Loaded` (evento: nessun argomento)
- [ ] `DataModel.MatchmakingType`: MatchmakingType  _(ReadOnly)_
- [ ] `DataModel.PlaceId`: int64  _(ReadOnly)_
- [ ] `DataModel.PlaceVersion`: int  _(ReadOnly)_
- [ ] `DataModel.PrivateServerId`: string  _(ReadOnly)_
- [ ] `DataModel.PrivateServerOwnerId`: int64  _(ReadOnly)_
- [ ] `DataModel.RunService`: RunService  _(ReadOnly)_
- [ ] `DataModel.ServerLifecycleChanged` (evento: serverLifecycleChangedEvent: Dictionary)
- [ ] `DataModel.ServerLowMemoryWarning` (evento: nessun argomento)
- [ ] `DataModel.ServerRegion`: string  _(ReadOnly)_
- [ ] `DataModel.ServerRestartScheduled` (evento: restartTime: DateTime, source: CloseReason, attributes: Dictionary)
- [ ] `DataModel.Workspace`: Workspace  _(ReadOnly)_
- [ ] `DataModel:BindToClose(function: Function)` -> null
- [ ] `DataModel:IsLoaded()` -> bool

### ServiceProvider (eredita da Instance) _NotCreatable_
- [ ] `ServiceProvider.Close` (evento: nessun argomento)
- [ ] `ServiceProvider.ServiceAdded` (evento: service: Instance)
- [ ] `ServiceProvider.ServiceRemoving` (evento: service: Instance)
- [ ] `ServiceProvider:FindService(className: string)` -> Instance
- [ ] `ServiceProvider:GetService(className: string)` -> Instance

### ReplicatedStorage (eredita da Instance) _NotCreatable, Service_
_(nessun membro proprio: vedi la classe da cui eredita)_

### ReplicatedFirst (eredita da Instance) _NotCreatable, Service_
- [ ] `ReplicatedFirst:RemoveDefaultLoadingScreen()` -> null

### ServerStorage (eredita da Instance) _NotCreatable, Service_
_(nessun membro proprio: vedi la classe da cui eredita)_

### ServerScriptService (eredita da Instance) _NotCreatable, Service_
_(nessun membro proprio: vedi la classe da cui eredita)_

### RemoteEvent (eredita da BaseRemoteEvent)
- [ ] `RemoteEvent.OnClientEvent` (evento: arguments: Tuple)
- [ ] `RemoteEvent.OnServerEvent` (evento: player: Player, arguments: Tuple)
- [ ] `RemoteEvent:FireAllClients(arguments: Tuple)` -> null
- [ ] `RemoteEvent:FireClient(player: Player, arguments: Tuple)` -> null
- [ ] `RemoteEvent:FireServer(arguments: Tuple)` -> null

### RemoteFunction (eredita da Instance)
- [ ] `RemoteFunction.OnClientInvoke` (callback: arguments: Tuple) -> Tuple
- [ ] `RemoteFunction.OnServerInvoke` (callback: player: Player, arguments: Tuple) -> Tuple
- [ ] `RemoteFunction:InvokeClient(player: Player, arguments: Tuple)` -> Tuple  _(Yields)_
- [ ] `RemoteFunction:InvokeServer(arguments: Tuple)` -> Tuple  _(Yields)_

### UnreliableRemoteEvent (eredita da BaseRemoteEvent)
- [ ] `UnreliableRemoteEvent.OnClientEvent` (evento: arguments: Tuple)
- [ ] `UnreliableRemoteEvent.OnServerEvent` (evento: player: Player, arguments: Tuple)
- [ ] `UnreliableRemoteEvent:FireAllClients(arguments: Tuple)` -> null
- [ ] `UnreliableRemoteEvent:FireClient(player: Player, arguments: Tuple)` -> null
- [ ] `UnreliableRemoteEvent:FireServer(arguments: Tuple)` -> null

### DataStoreService (eredita da Instance) _NotCreatable, Service_
- [ ] `DataStoreService:GetDataStore(name: string, scope: string = global, options: Instance = nil)` -> DataStore
- [ ] `DataStoreService:GetGlobalDataStore()` -> DataStore
- [ ] `DataStoreService:GetOrderedDataStore(name: string, scope: string = global)` -> OrderedDataStore
- [ ] `DataStoreService:GetRequestBudgetForRequestType(requestType: DataStoreRequestType)` -> int
- [ ] `DataStoreService:ListDataStoresAsync(prefix: string = , pageSize: int = 0, cursor: string = )` -> DataStoreListingPages  _(Yields)_
- [ ] `DataStoreService:SetRateLimitForRequestType(requestType: DataStoreRequestType, baseLimit: int, perPlayerLimit: int)` -> null

### GlobalDataStore (eredita da Instance) _NotCreatable_
- [ ] `GlobalDataStore:BatchGetAsync(keys: Array, options: Dictionary = nil)` -> Dictionary  _(Yields)_
- [ ] `GlobalDataStore:GetAsync(key: string, options: DataStoreGetOptions = nil)` -> Tuple  _(Yields)_
- [ ] `GlobalDataStore:IncrementAsync(key: string, delta: int = 1, userIds: Array = {}, options: DataStoreIncrementOptions = nil)` -> Variant  _(Yields)_
- [ ] `GlobalDataStore:RemoveAsync(key: string)` -> Tuple  _(Yields)_
- [ ] `GlobalDataStore:SetAsync(key: string, value: Variant, userIds: Array = {}, options: DataStoreSetOptions = nil)` -> Variant  _(Yields)_
- [ ] `GlobalDataStore:UpdateAsync(key: string, transformFunction: Function)` -> Tuple  _(Yields)_

### DataStore (eredita da GlobalDataStore) _NotCreatable_
- [ ] `DataStore:GetVersionAsync(key: string, version: string)` -> Tuple  _(Yields)_
- [ ] `DataStore:GetVersionAtTimeAsync(key: string, timestamp: int64)` -> Tuple  _(Yields)_
- [ ] `DataStore:ListKeysAsync(prefix: string = , pageSize: int = 0, cursor: string = , excludeDeleted: bool = false)` -> DataStoreKeyPages  _(Yields)_
- [ ] `DataStore:ListVersionsAsync(key: string, sortDirection: SortDirection = Ascending, minDate: int64 = 0, maxDate: int64 = 0, pageSize: int = 0)` -> DataStoreVersionPages  _(Yields)_

### OrderedDataStore (eredita da GlobalDataStore) _NotCreatable_
- [ ] `OrderedDataStore:GetSortedAsync(ascending: bool, pagesize: int, minValue: Variant, maxValue: Variant)` -> DataStorePages  _(Yields)_

### Pages (eredita da Instance) _NotCreatable_
- [ ] `Pages.IsFinished`: bool  _(ReadOnly)_
- [ ] `Pages:AdvanceToNextPageAsync()` -> null  _(Yields)_
- [ ] `Pages:GetCurrentPage()` -> Array

### DataStorePages (eredita da Pages) _NotCreatable_
_(nessun membro proprio: vedi la classe da cui eredita)_

### DataStoreKeyPages (eredita da Pages) _NotCreatable_
- [ ] `DataStoreKeyPages.Cursor`: string  _(ReadOnly)_

### DataStoreKeyInfo (eredita da Instance) _NotCreatable_
- [ ] `DataStoreKeyInfo.CreatedTime`: int64  _(ReadOnly)_
- [ ] `DataStoreKeyInfo.UpdatedTime`: int64  _(ReadOnly)_
- [ ] `DataStoreKeyInfo.Version`: string  _(ReadOnly)_
- [ ] `DataStoreKeyInfo:GetMetadata()` -> Dictionary
- [ ] `DataStoreKeyInfo:GetUserIds()` -> Array

### DataStoreSetOptions (eredita da Instance)
- [ ] `DataStoreSetOptions:GetMetadata()` -> Dictionary
- [ ] `DataStoreSetOptions:SetMetadata(attributes: Dictionary)` -> null

### MemoryStoreService (eredita da Instance) _Service_
- [ ] `MemoryStoreService:GetDistributedCounter(name: string)` -> MemoryStoreDistributedCounter
- [ ] `MemoryStoreService:GetHashMap(name: string)` -> MemoryStoreHashMap
- [ ] `MemoryStoreService:GetQueue(name: string, invisibilityTimeout: int = 30)` -> MemoryStoreQueue
- [ ] `MemoryStoreService:GetSortedMap(name: string)` -> MemoryStoreSortedMap

### MemoryStoreSortedMap (eredita da Instance) _NotCreatable_
- [ ] `MemoryStoreSortedMap:GetAsync(key: string)` -> Tuple  _(Yields)_
- [ ] `MemoryStoreSortedMap:GetRangeAsync(direction: SortDirection, count: int, exclusiveLowerBound: Variant, exclusiveUpperBound: Variant)` -> Array  _(Yields)_
- [ ] `MemoryStoreSortedMap:GetSizeAsync()` -> int  _(Yields)_
- [ ] `MemoryStoreSortedMap:RemoveAsync(key: string)` -> null  _(Yields)_
- [ ] `MemoryStoreSortedMap:SetAsync(key: string, value: Variant, expiration: int64, sortKey: Variant)` -> bool  _(Yields)_
- [ ] `MemoryStoreSortedMap:UpdateAsync(key: string, transformFunction: Function, expiration: int64)` -> Tuple  _(Yields)_

### MemoryStoreQueue (eredita da Instance) _NotCreatable_
- [ ] `MemoryStoreQueue:AddAsync(value: Variant, expiration: int64, priority: double = 0)` -> null  _(Yields)_
- [ ] `MemoryStoreQueue:GetSizeAsync(excludeInvisible: bool = false)` -> int  _(Yields)_
- [ ] `MemoryStoreQueue:ReadAsync(count: int, allOrNothing: bool = false, waitTimeout: double = -1)` -> Tuple  _(Yields)_
- [ ] `MemoryStoreQueue:RemoveAsync(id: string)` -> null  _(Yields)_

### MemoryStoreHashMap (eredita da Instance) _NotCreatable_
- [ ] `MemoryStoreHashMap:GetAsync(key: string)` -> Variant  _(Yields)_
- [ ] `MemoryStoreHashMap:ListItemsAsync(count: int)` -> MemoryStoreHashMapPages  _(Yields)_
- [ ] `MemoryStoreHashMap:RemoveAsync(key: string)` -> null  _(Yields)_
- [ ] `MemoryStoreHashMap:SetAsync(key: string, value: Variant, expiration: int64)` -> bool  _(Yields)_
- [ ] `MemoryStoreHashMap:UpdateAsync(key: string, transformFunction: Function, expiration: int64)` -> Variant  _(Yields)_

### MessagingService (eredita da Instance) _NotCreatable, Service_
- [ ] `MessagingService:PublishAsync(topic: string, message: Variant)` -> null  _(Yields)_
- [ ] `MessagingService:SubscribeAsync(topic: string, callback: Function)` -> RBXScriptConnection  _(Yields)_

### HttpService (eredita da Instance) _NotCreatable, Service_
- [ ] `HttpService.HttpEnabled`: bool = false
- [ ] `HttpService:CreateWebStreamClient(streamClientType: WebStreamClientType, requestOptions: Dictionary)` -> WebStreamClient
- [ ] `HttpService:GenerateGUID(wrapInCurlyBraces: bool = true)` -> string
- [ ] `HttpService:GetAsync(url: Variant, nocache: bool = false, headers: Variant)` -> string  _(Yields)_
- [ ] `HttpService:GetSecret(key: string)` -> Secret
- [ ] `HttpService:JSONDecode(input: string)` -> Variant  _(CustomLuaState)_
- [ ] `HttpService:JSONEncode(input: Variant)` -> string  _(CustomLuaState)_
- [ ] `HttpService:PostAsync(url: Variant, data: string, content_type: HttpContentType = ApplicationJson, compress: bool = false, headers: Variant)` -> string  _(Yields)_
- [ ] `HttpService:RequestAsync(requestOptions: Dictionary)` -> Dictionary  _(Yields)_
- [ ] `HttpService:UrlEncode(input: string)` -> string

### MarketplaceService (eredita da Instance) _NotCreatable, Service_
- [ ] `MarketplaceService.ProcessReceipt` (callback: receiptInfo: Dictionary) -> ProductPurchaseDecision
- [ ] `MarketplaceService.PromptBulkPurchaseFinished` (evento: player: Instance, status: MarketplaceBulkPurchasePromptStatus, results: Dictionary)
- [ ] `MarketplaceService.PromptBundlePurchaseFinished` (evento: player: Instance, bundleId: int64, wasPurchased: bool)
- [ ] `MarketplaceService.PromptGamePassPurchaseFinished` (evento: player: Instance, gamePassId: int64, wasPurchased: bool)
- [ ] `MarketplaceService.PromptPremiumPurchaseFinished` (evento: nessun argomento)
- [ ] `MarketplaceService.PromptProductPurchaseFinished` (evento: userId: int64, productId: int64, isPurchased: bool)
- [ ] `MarketplaceService.PromptPurchaseFinished` (evento: player: Instance, assetId: int64, isPurchased: bool)
- [ ] `MarketplaceService.PromptRobloxSubscriptionPurchaseFinished` (evento: user: Player, didTryPurchasing: bool)
- [ ] `MarketplaceService.PromptSubscriptionPurchaseFinished` (evento: user: Player, subscriptionId: string, didTryPurchasing: bool)
- [ ] `MarketplaceService:BindReceiptHandler(transactionType: ReceiptType, handler: Function, filter: Array?)` -> RBXScriptConnection
- [ ] `MarketplaceService:CanUsersTransferRobuxAsync(sender: Player, receiverUser: User)` -> RobuxTransferEligibility  _(Yields)_
- [ ] `MarketplaceService:GetDeveloperProductsAsync()` -> Instance  _(Yields)_
- [ ] `MarketplaceService:GetPassOwnershipAsync(user: Player, gamePassId: int64)` -> Dictionary  _(Yields)_
- [ ] `MarketplaceService:GetProductInfoAsync(assetId: int64, infoType: InfoType = Asset)` -> Dictionary  _(Yields)_
- [ ] `MarketplaceService:GetRobloxSubscriptionDetailsAsync(user: Player)` -> Dictionary  _(Yields)_
- [ ] `MarketplaceService:GetSubscriptionProductInfoAsync(subscriptionId: string)` -> Dictionary  _(Yields)_
- [ ] `MarketplaceService:GetUserSubscriptionDetailsAsync(user: Player, subscriptionId: string)` -> Dictionary  _(Yields)_
- [ ] `MarketplaceService:GetUserSubscriptionPaymentHistoryAsync(user: Player, subscriptionId: string)` -> Array  _(Yields)_
- [ ] `MarketplaceService:GetUserSubscriptionStatusAsync(user: Player, subscriptionId: string)` -> Dictionary  _(Yields)_
- [ ] `MarketplaceService:GetUsersPriceLevelsAsync(userIds: Array)` -> Array  _(Yields)_
- [ ] `MarketplaceService:OpenShop(player: Player)` -> null
- [ ] `MarketplaceService:PlayerOwnsAssetAsync(player: Instance, assetId: int64)` -> bool  _(Yields)_
- [ ] `MarketplaceService:PlayerOwnsBundleAsync(player: Player, bundleId: int64)` -> bool  _(Yields)_
- [ ] `MarketplaceService:PromptBulkPurchase(player: Player, lineItems: Array, options: Dictionary)` -> null
- [ ] `MarketplaceService:PromptBundlePurchase(player: Instance, bundleId: int64)` -> null
- [ ] `MarketplaceService:PromptCancelSubscription(user: Player, subscriptionId: string)` -> null
- [ ] `MarketplaceService:PromptGamePassPurchase(player: Instance, gamePassId: int64)` -> null
- [ ] `MarketplaceService:PromptProductPurchase(player: Instance, productId: int64, equipIfPurchased: bool = true, currencyType: CurrencyType = Default)` -> null
- [ ] `MarketplaceService:PromptPurchase(player: Instance, assetId: int64, equipIfPurchased: bool = true, currencyType: CurrencyType = Default)` -> null
- [ ] `MarketplaceService:PromptRobloxSubscriptionPurchase(user: Player)` -> null
- [ ] `MarketplaceService:PromptRobuxTransferAsync(sender: Player, receiverUserId: int64, amount: int64)` -> string  _(Yields)_
- [ ] `MarketplaceService:PromptSubscriptionPurchase(user: Player, subscriptionId: string)` -> null
- [ ] `MarketplaceService:PromptVIPServerPurchase(player: Player)` -> null
- [ ] `MarketplaceService:RankProductsAsync(productIdentifiers: Array)` -> Array  _(Yields)_
- [ ] `MarketplaceService:RecommendTopProductsAsync(infoTypes: Array)` -> Array  _(Yields)_
- [ ] `MarketplaceService:UserOwnsGamePassAsync(userId: User, gamePassId: int64)` -> bool  _(Yields)_

### BadgeService (eredita da Instance) _NotCreatable, Service_
- [ ] `BadgeService:AwardBadgeAsync(userId: User, badgeId: int64)` -> bool  _(Yields)_
- [ ] `BadgeService:CheckUserBadgesAsync(userId: User, badgeIds: Array)` -> Array  _(Yields)_
- [ ] `BadgeService:GetBadgeInfoAsync(badgeId: int64)` -> Dictionary  _(Yields)_
- [ ] `BadgeService:GetUserBadgesAsync(userId: User, badgeIds: Array)` -> Array  _(Yields)_
- [ ] `BadgeService:UserHasBadgeAsync(userId: User, badgeId: int64)` -> bool  _(Yields)_

### GroupService (eredita da Instance) _NotCreatable, Service_
- [ ] `GroupService:GetAlliesAsync(groupId: int64)` -> StandardPages  _(Yields)_
- [ ] `GroupService:GetEnemiesAsync(groupId: int64)` -> StandardPages  _(Yields)_
- [ ] `GroupService:GetGroupInfoAsync(groupId: int64)` -> Variant  _(Yields)_
- [ ] `GroupService:GetGroupsAsync(userId: User)` -> Array  _(Yields)_
- [ ] `GroupService:GetRolesInGroupAsync(userId: User, groupId: int64)` -> Variant  _(Yields)_
- [ ] `GroupService:PromptJoinAsync(groupId: int64)` -> GroupMembershipStatus  _(Yields)_

### PolicyService (eredita da Instance) _NotCreatable, Service_
- [ ] `PolicyService:CanViewBrandProjectAsync(player: Player, brandProjectId: string)` -> bool  _(Yields)_
- [ ] `PolicyService:GetPolicyInfoForPlayerAsync(player: Instance)` -> Dictionary  _(Yields)_

### LocalizationService (eredita da Instance) _NotCreatable, Service_
- [ ] `LocalizationService.RobloxLocaleId`: string =   _(ReadOnly)_
- [ ] `LocalizationService.SystemLocaleId`: string =   _(ReadOnly)_
- [ ] `LocalizationService:GetCorescriptLocalizations()` -> Instances
- [ ] `LocalizationService:GetCountryRegionForPlayerAsync(player: Instance)` -> string  _(Yields)_
- [ ] `LocalizationService:GetTableEntries(instance: Instance = nil)` -> Array
- [ ] `LocalizationService:GetTranslatorForLocaleAsync(locale: string)` -> Instance  _(Yields)_
- [ ] `LocalizationService:GetTranslatorForPlayerAsync(player: Instance)` -> Instance  _(Yields)_

### TextChatService (eredita da Instance) _NotCreatable, Service_
- [ ] `TextChatService.BubbleDisplayed` (evento: partOrCharacter: Instance, textChatMessage: TextChatMessage)
- [ ] `TextChatService.ChatTranslationEnabled`: bool = true
- [ ] `TextChatService.ChatVersion`: ChatVersion = LegacyChatService
- [ ] `TextChatService.CreateDefaultCommands`: bool = true
- [ ] `TextChatService.CreateDefaultTextChannels`: bool = true
- [ ] `TextChatService.MessageReceived` (evento: textChatMessage: TextChatMessage)
- [ ] `TextChatService.OnBubbleAdded` (callback: message: TextChatMessage, adornee: Instance) -> Tuple
- [ ] `TextChatService.OnChatWindowAdded` (callback: message: TextChatMessage) -> Tuple
- [ ] `TextChatService.OnIncomingMessage` (callback: message: TextChatMessage) -> Tuple
- [ ] `TextChatService.SendingMessage` (evento: textChatMessage: TextChatMessage)
- [ ] `TextChatService:CanUserChatAsync(userId: User)` -> bool  _(Yields)_
- [ ] `TextChatService:CanUsersChatAsync(userIdFrom: User, userIdTo: User)` -> bool  _(Yields)_
- [ ] `TextChatService:CanUsersDirectChatAsync(requesterUserId: User, userIds: Array)` -> Array  _(Yields)_
- [ ] `TextChatService:DisplayBubble(partOrCharacter: Instance, message: string)` -> null
- [ ] `TextChatService:GetChatGroupsAsync(players: Instances)` -> Array  _(Yields)_

### TextChannel (eredita da Instance)
- [ ] `TextChannel.AddPlayersOnJoin`: bool = false
- [ ] `TextChannel.DirectChatRequester`: Player  _(ReadOnly)_
- [ ] `TextChannel.MessageReceived` (evento: incomingMessage: TextChatMessage)
- [ ] `TextChannel.OnIncomingMessage` (callback: message: TextChatMessage) -> Tuple
- [ ] `TextChannel.ShouldDeliverCallback` (callback: message: TextChatMessage, textSource: TextSource) -> Tuple
- [ ] `TextChannel:AddUserAsync(userId: User)` -> Tuple  _(Yields)_
- [ ] `TextChannel:DisplaySystemMessage(systemMessage: string, metadata: string = )` -> TextChatMessage
- [ ] `TextChannel:SendAsync(message: string, metadata: string = )` -> TextChatMessage  _(Yields)_
- [ ] `TextChannel:SetDirectChatRequester(requester: Player)` -> null

### TextChatCommand (eredita da Instance)
- [ ] `TextChatCommand.AutocompleteVisible`: bool = true
- [ ] `TextChatCommand.Enabled`: bool = true
- [ ] `TextChatCommand.PrimaryAlias`: string = 
- [ ] `TextChatCommand.SecondaryAlias`: string = 
- [ ] `TextChatCommand.Triggered` (evento: originTextSource: TextSource, unfilteredText: string)

### TextChatMessage (eredita da Instance) _NotCreatable_
- [ ] `TextChatMessage.BubbleChatMessageProperties`: BubbleChatMessageProperties
- [ ] `TextChatMessage.ChatWindowMessageProperties`: ChatWindowMessageProperties
- [ ] `TextChatMessage.MessageId`: string = 
- [ ] `TextChatMessage.Metadata`: string = 
- [ ] `TextChatMessage.PrefixText`: string = 
- [ ] `TextChatMessage.Status`: TextChatMessageStatus = Unknown
- [ ] `TextChatMessage.TextChannel`: TextChannel
- [ ] `TextChatMessage.TextSource`: TextSource
- [ ] `TextChatMessage.Text`: string = 
- [ ] `TextChatMessage.Timestamp`: DateTime
- [ ] `TextChatMessage.Translation`: string = 

### TextSource (eredita da Instance) _NotCreatable_
- [ ] `TextSource.CanSend`: bool = true
- [ ] `TextSource.UserId`: int64 = 0  _(ReadOnly)_

### TeleportService (eredita da Instance) _NotCreatable, Service_
- [ ] `TeleportService.LocalPlayerArrivedFromTeleport` (evento: loadingGui: Instance, dataTable: Variant)
- [ ] `TeleportService.TeleportInitFailed` (evento: player: Instance, teleportResult: TeleportResult, errorMessage: string, placeId: int64, teleportOptions: Instance)
- [ ] `TeleportService:GetArrivingTeleportGui()` -> Instance
- [ ] `TeleportService:GetLocalPlayerTeleportData()` -> Variant
- [ ] `TeleportService:GetPlayerPlaceInstanceAsync(userId: User)` -> Tuple  _(Yields)_
- [ ] `TeleportService:GetTeleportSetting(setting: string)` -> Variant
- [ ] `TeleportService:PromptExperienceDetailsAsync(player: Player, universeId: int64)` -> PromptExperienceDetailsResult  _(Yields)_
- [ ] `TeleportService:SetTeleportGui(gui: Instance)` -> null
- [ ] `TeleportService:SetTeleportSetting(setting: string, value: Variant)` -> null
- [ ] `TeleportService:Teleport(placeId: int64, player: Instance = nil, teleportData: Variant, customLoadingScreen: Instance = nil)` -> null
- [ ] `TeleportService:TeleportAsync(placeId: int64, players: Instances, teleportOptions: Instance = nil)` -> Instance  _(Yields)_

### TeleportOptions (eredita da Instance)
- [ ] `TeleportOptions.ReservedServerId`: string = 
- [ ] `TeleportOptions.ServerInstanceId`: string = 
- [ ] `TeleportOptions.VipServerId`: string = 
- [ ] `TeleportOptions:GetTeleportData()` -> Variant
- [ ] `TeleportOptions:SetTeleportData(teleportData: Variant)` -> null

### SocialService (eredita da Instance) _NotCreatable, Service_
- [ ] `SocialService.CallInviteStateChanged` (evento: player: Instance, inviteState: InviteState)
- [ ] `SocialService.GameInvitePromptClosed` (evento: player: Instance, recipientIds: Array)
- [ ] `SocialService.OnCallInviteInvoked` (callback: tag: string, callParticipantIds: Array) -> Instance
- [ ] `SocialService.PhoneBookPromptClosed` (evento: player: Instance)
- [ ] `SocialService.ShareSheetClosed` (evento: player: Player)
- [ ] `SocialService:CanSendCallInviteAsync(player: Instance)` -> bool  _(Yields)_
- [ ] `SocialService:CanSendGameInviteAsync(player: Instance, recipientId: User = U1.AQAAAAAAAAAAAAAAAAAAAAA)` -> bool  _(Yields)_
- [ ] `SocialService:GetEventRsvpStatusAsync(eventId: string)` -> RsvpStatus  _(Yields)_
- [ ] `SocialService:GetExperienceEventAsync(eventId: string)` -> Dictionary?  _(Yields)_
- [ ] `SocialService:GetPartyAsync(partyId: string)` -> Array  _(Yields)_
- [ ] `SocialService:GetPlayersByPartyId(partyId: string)` -> Instances
- [ ] `SocialService:GetUpcomingExperienceEventsAsync()` -> Array  _(Yields)_
- [ ] `SocialService:HideSelfView()` -> null
- [ ] `SocialService:PromptFeedbackSubmissionAsync(options: Dictionary?)` -> null  _(Yields)_
- [ ] `SocialService:PromptGameInvite(player: Instance, experienceInviteOptions: Instance = nil)` -> null
- [ ] `SocialService:PromptLinkSharingAsync(player: Player, options: Dictionary = nil)` -> Tuple  _(Yields)_
- [ ] `SocialService:PromptPhoneBook(player: Instance, tag: string)` -> null
- [ ] `SocialService:PromptRateExperienceAsync()` -> ExperienceRatingResult  _(Yields)_
- [ ] `SocialService:PromptRsvpToEventAsync(eventId: string)` -> RsvpStatus  _(Yields)_
- [ ] `SocialService:ShowSelfView(selfViewPosition: SelfViewPosition = LastPosition)` -> null

### Chat (eredita da Instance) _NotCreatable, Service_
- [ ] `Chat.BubbleChatEnabled`: bool = false
- [ ] `Chat.Chatted` (evento: part: Instance, message: string, color: ChatColor)
- [ ] `Chat.LoadDefaultChat`: bool = true
- [ ] `Chat:CanUserChatAsync(userId: int64)` -> bool  _(Yields)_
- [ ] `Chat:CanUsersChatAsync(userIdFrom: int64, userIdTo: int64)` -> bool  _(Yields)_
- [ ] `Chat:Chat(partOrCharacter: Instance, message: string, color: ChatColor = Blue)` -> null
- [ ] `Chat:FilterStringAsync(stringToFilter: string, playerFrom: Player, playerTo: Player)` -> string  _(Yields)_
- [ ] `Chat:FilterStringForBroadcast(stringToFilter: string, playerFrom: Player)` -> string  _(Yields)_
- [ ] `Chat:InvokeChatCallback(callbackType: ChatCallbackType, callbackArguments: Tuple)` -> Tuple
- [ ] `Chat:RegisterChatCallback(callbackType: ChatCallbackType, callbackFunction: Function)` -> null
- [ ] `Chat:SetBubbleChatSettings(settings: Variant)` -> null
