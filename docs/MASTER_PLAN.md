# GTA Crime Overhaul — Zero-to-Complete Master Plan

This is the implementation order. The project is deliberately built as vertical production stages, not as a giant collection of disconnected scripts.

## Completion definition

The overhaul is considered feature-complete when a player can prepare and commit free-roam crimes ranging from convenience-store robberies through major bank jobs; witnesses, masks, clothing, vehicles and plates affect identification; police can investigate after the player leaves; cases and warrants persist; owned vehicles and weapons matter; loot must be physically extracted and secured; businesses react over time; and the money earned has meaningful uses.

---

# Stage 0 — Repository, native ASI and engineering foundation

**Goal:** a stable native plugin that loads, logs, saves versioned data and can be built reproducibly.

1. Initialize the GitHub repository and lock Story Mode/offline scope.
2. Define `GTA_Crime_Overhaul.asi` as the authoritative release binary.
3. Add `docs/DESIGN_LOCKS.md` and require later design changes to update it explicitly.
4. Add `docs/FEASIBILITY_MATRIX.md` so engine limitations are documented before content work.
5. Add native C++20 project structure.
6. Add Script Hook V SDK discovery to CMake.
7. Never commit/redistribute `ScriptHookV.dll` or `dinput8.dll`.
8. Add x64 Release/Debug build configurations.
9. Configure output suffix to `.asi` rather than `.dll`.
10. Add Script Hook V `scriptRegister`/`scriptUnregister` lifecycle.
11. Add `ScriptMain` fiber loop and `WAIT(0)` scheduling.
12. Add structured file logging with timestamps and severity.
13. Add crash-safe top-level exception guards around our tick systems where practical.
14. Add project version, save-schema version and build information constants.
15. Add config directory discovery relative to GTA V executable/plugin location.
16. Add `GTA_Crime_Overhaul.ini` or project config file with debug and feature toggles.
17. Add JSON persistence abstraction without persisting raw GTA entity handles.
18. Add atomic save strategy: write temporary file, validate, then replace.
19. Add save corruption fallback/backup behavior.
20. Add deterministic IDs for project-owned cases, businesses, clerks, vehicles and loot containers.
21. Add lightweight event bus so systems do not hard-call one another everywhere.
22. Add scheduler with frame, 5 Hz, 2 Hz, 1 Hz and event-driven work queues.
23. Add mission/cutscene compatibility gate interface.
24. Add developer/debug commands and on-screen diagnostics toggle.
25. Add GitHub Actions build workflow for Windows x64.
26. Make CI obtain the Script Hook V developer SDK from the official source instead of redistributing it.
27. Package only this mod's ASI/config/data/docs into CI artifacts.
28. Add release ZIP layout matching the GTA V root installation flow.
29. Add installation and troubleshooting docs.
30. **Exit criteria:** a generated `GTA_Crime_Overhaul.asi` loads in Story Mode, writes a startup log, ticks safely, creates/loads a schema-versioned save and unloads cleanly.

---

# Stage 1 — Shared world abstraction

**Goal:** stop gameplay code from scattering raw native calls and handles everywhere.

31. Add wrappers for player ped, position, alive/dead, wanted state and mission state.
32. Add wrappers for safe ped/entity existence checks.
33. Add world-query helpers that retrieve only bounded nearby entities.
34. Add distance/grid filtering before expensive line-of-sight work.
35. Add `PedSnapshot` containing model, outfit/component signature, props, state and location.
36. Add `VehicleSnapshot` containing model, colors, plate, plate style, position and project vehicle ID if owned.
37. Add animation dictionary request/release helper with timeout.
38. Add task/animation cancellation recovery.
39. Add prop attach/detach helper with guaranteed cleanup.
40. Add door/interior adapter interface.
41. Add blip/UI abstraction.
42. Add subtitle/help-text abstraction.
43. Add audio/speech abstraction.
44. Add input mapping abstraction for keyboard/controller.
45. Add spatial debug rendering for witness FOV, crime zones and search areas.
46. **Exit criteria:** game-specific native details can be changed in adapters without rewriting robbery/law domain logic.

---

# Stage 2 — Crime domain and persistent case model

**Goal:** create the central simulation all robberies feed.

47. Implement `CrimeDirector`.
48. Implement `CrimeRegistry` with unique crime/case IDs.
49. Define crime types: robbery, armed robbery, assault, homicide, officer assault, vehicle theft, property damage and later extensible types.
50. Define crime severity and escalation rules.
51. Define crime location, target business and time metadata.
52. Define immediate response state separately from long-term case state.
53. Define the canonical state flow: unobserved → observed → reporting → reported → investigating → unknown/identified suspect → BOLO/warrant → pursuit/search → dormant/resolved.
54. Add evidence records with source, confidence, immutable observation snapshot and timestamp.
55. Add case merge rules so several observations from one event strengthen a single case rather than create duplicates.
56. Add case expiry/decay policy for weak low-level evidence without deleting major cases.
57. Add save/load for cases.
58. Add debug case inspector.
59. **Exit criteria:** synthetic debug crimes can create, persist, reload and transition a case without any store-specific code.

---

# Stage 3 — One perfect convenience store vertical slice

**Goal:** prove robbery gameplay before scaling locations.

60. Pick one 24/7/convenience store as the prototype.
61. Define business volume, register positions, clerk anchor, customer zone, exits and optional safe area in data.
62. Detect normal entry without starting a robbery.
63. Detect robbery threshold such as sustained weapon aim/threat against the clerk.
64. Create a robbery session only after that threshold.
65. Give the logical clerk a persistent ID/profile.
66. Add clerk personalities: cowardly, compliant, defiant, panicky, armed, experienced and reckless.
67. Add hands-up/cower/flee/phone/panic-button behavior using GTA-compatible tasks/animations.
68. Add robbery demands: open register, open second register, empty safe, hands up, get down, move away from alarm, don't move.
69. Add compliant and defiant branches.
70. Add secret alarm branch.
71. Add clerk weapon/fight branch for appropriate personalities.
72. Add lying/partial-cash behavior where appropriate.
73. Add cash source state rather than instant arbitrary payout.
74. Add robbery timeout/abort behavior if player leaves or clerk becomes invalid.
75. Add cleanup/recovery if animation dictionary fails, NPC dies or GTA AI interrupts a task.
76. **Exit criteria:** one store supports multiple believable robbery outcomes and always recovers without a stuck clerk/session.

---

# Stage 4 — Witness perception and reporting

**Goal:** witnesses know only what they can plausibly perceive.

77. Implement `WitnessDirector` using bounded nearby candidate scans.
78. Stagger candidate updates at low frequency rather than raycasting every ped every frame.
79. Add distance/FOV/occlusion checks.
80. Add hearing-based awareness for gunshots/shouts separately from visual identification.
81. Capture whether witness saw face.
82. Capture whether witness saw a mask/bandana.
83. Capture outfit/clothing signature.
84. Capture weapon class if visible.
85. Capture vehicle model and color if observed.
86. Capture plate if geometry/visibility/time support it.
87. Capture last-known direction/location.
88. Add confidence based on distance, view duration, lighting/visibility approximation and obstruction.
89. Add witness emotional state: fear, panic, defiance.
90. Add reactions: freeze, hide, flee, call police, use panic button, shout, or fail/refuse to report.
91. Add delayed reporting so escape before the call completes matters.
92. Allow interruption of a reporting witness through normal gameplay without globally erasing all evidence.
93. Allow additional witnesses to observe violence against a witness.
94. Persist important witness observations into the case; do not persist every random ambient ped forever.
95. **Exit criteria:** two people in the same store can give different reports based on what each actually saw.

---

# Stage 5 — Identity, masks, clothing and recognition

**Goal:** create RDR2/Mafia-style identity gameplay within GTA V limits.

96. Implement `IdentitySystem`.
97. Build a stable outfit signature from relevant GTA ped components/props.
98. Detect project-approved mask/bandana state.
99. Reduce new face evidence while properly masked.
100. Preserve face evidence gathered before the mask went on.
101. Preserve clothing evidence while masked.
102. Preserve vehicle/plate evidence while masked.
103. Add outfit changes as a clothing-match countermeasure.
104. Ensure changing clothing does not clear confirmed face identification.
105. Add character identity rules for Franklin/Michael/Trevor/free-mode-compatible supported player models as applicable.
106. Add recognition thresholds for witness memory and police encounter checks.
107. Add repeat-clerk recognition when prior face evidence is strong.
108. Add subtle recognition behavior before alarm escalation: stare/pause/back-away/silent alarm possibilities.
109. Add a GTA-compatible visual bandana/mask path using existing GTA assets or optional custom assets.
110. Never redistribute RDR2 model/animation/audio assets.
111. **Exit criteria:** rob unmasked → clerk knows face; rob masked → clerk may know clothing/vehicle but not face; mask added after face exposure does not erase ID.

---

# Stage 6 — Vehicle identity and garage license-plate system

**Goal:** make getaway vehicles and plates persistent evidence.

112. Implement `VehicleIdentitySystem`.
113. Create persistent `OwnedVehicleRecord` ID separate from GTA entity handles.
114. Save model, modifications needed for reconstruction, colors, plate, plate style, storage and status.
115. Record vehicle model/color/plate observations as immutable case evidence.
116. Track a separate `vehicleBOLO` from person warrant state.
117. Add garage/mod-shop **Change License Plate** service.
118. Validate allowed plate length/characters against GTA V constraints.
119. Charge money for the plate-change service.
120. Apply the plate to the current GTA vehicle using vehicle natives.
121. Save the new plate string/style to the owned vehicle record.
122. Keep old case evidence unchanged after the plate is altered.
123. Make a new plate defeat direct old-plate matching unless stronger evidence already links the physical vehicle/owner.
124. Add paint-change evidence behavior: old reports keep old color snapshot.
125. Add vehicle swapping logic: if police/witnesses see the swap, continuity can be preserved; if not, it can break a search trail.
126. Add stolen/temporary getaway vehicle records that do not automatically become owned.
127. Add vehicle crime-history flags used for police recognition and impound logic.
128. **Exit criteria:** commit crime in known car → plate/model BOLO exists → change plate/repaint in garage → current vehicle changes while the historical report stays historically correct.

---

# Stage 7 — Police dispatch, investigation and interviews

**Goal:** police investigate the place where the crime happened, even after the player escapes.

129. Implement `DispatchDirector` separate from GTA's built-in wanted-star response.
130. Determine response type from robbery severity, weapon use, shots, injuries/deaths and officer attacks.
131. Spawn/task patrol response only when appropriate and compatible with GTA streaming.
132. Have officers navigate to the business/crime area, not directly to the hidden player.
133. Build temporary scene perimeter behavior.
134. Add officer tasks for checking the interior, bodies, abandoned vehicle and exits.
135. Add `InvestigationDirector`.
136. Select surviving/reporting witnesses for interviews.
137. Position/facing/idle gestures for officer-witness interaction using robust GTA tasks rather than mandatory cinematics.
138. Query actual witness observations from the case.
139. Generate interview semantic events such as `face_seen`, `face_not_seen`, `vehicle_seen`, `plate_seen`, `direction_known`.
140. Render suitable base-game speech when available, otherwise original audio/subtitle fallback.
141. Add information from interviews to case confidence/BOLO descriptions.
142. Keep crime scene alive while nearby; abstract it to a record when far away.
143. Reconstruct a lightweight scene if the player returns before scene expiry.
144. Add scene cleanup and business recovery timers.
145. **Exit criteria:** player can rob and leave before cops arrive, then circle back later and see officers investigating/interviewing rather than cops psychically chasing them.

---

# Stage 8 — Search, pursuit, warrants and surrender

**Goal:** replace instant-forget/psychic-search behavior with persistent consequences.

146. Implement `PursuitDirector` adapter around immediate GTA wanted behavior.
147. Track last-known player/vehicle location and direction when actually observed.
148. Create search area around the last reliable observation.
149. Bias patrol search routes toward exits/roads consistent with known heading.
150. Stop updating search center when police lose line of sight unless a new source reports the player.
151. Add current-outfit/current-vehicle matching during encounters.
152. Add unknown suspect state after escape if identity was never established.
153. Add identified suspect state and persistent warrant if identity crossed threshold.
154. Add separate person warrant and vehicle BOLO persistence.
155. Add police re-recognition of an identified suspect outside an active robbery.
156. Add traffic/vehicle recognition opportunities for wanted plates/models with fair visibility/proximity rules.
157. Add surrender prompt/state when police have control of the situation.
158. Add arrest transition.
159. Add fines/legal consequences by crime severity.
160. Add confiscation of unsecured robbery proceeds.
161. Add impound of involved owned vehicles where appropriate.
162. Add case resolution/served-warrant rules after arrest without wiping unrelated active cases.
163. **Exit criteria:** the player can genuinely disappear by breaking observation and changing appearance/vehicle intelligently, but an identified criminal can still have consequences later.

---

# Stage 9 — Persistent clerk/business memory and security escalation

**Goal:** make repeated robberies alter the world.

164. Implement `BusinessMemory`.
165. Persist robberies, last robbery time, money lost, violence severity, surviving logical clerks, recognized identities, known vehicles, security level and police attention.
166. Represent a clerk as a logical profile so GTA streaming can despawn/respawn the physical ped.
167. Restore/recreate a compatible clerk profile when the store streams back in.
168. Make recognized repeat robber return trigger state-dependent reactions.
169. Add business recovery downtime/cooldown after robbery.
170. Security level 0: normal store.
171. Security level 1: faster panic/alarm response.
172. Security level 2: improved camera coverage/evidence potential.
173. Security level 3: chance of security guard.
174. Security level 4: less exposed register cash / better safe behavior.
175. Balance escalation so a store remains playable and can eventually cool down.
176. If a clerk is killed, remove that personal witness memory while preserving business/crime records.
177. **Exit criteria:** repeatedly robbing one store changes future behavior and the world does not reset completely after losing stars.

---

# Stage 10 — CCTV and additional evidence

**Goal:** add non-human evidence without pretending GTA V has a real forensic engine.

178. Define security-camera coverage zones and logical camera records.
179. Track camera active/destroyed/disabled state.
180. Capture face only if unmasked and camera criteria are met.
181. Capture clothing.
182. Capture vehicle approach/departure when camera coverage supports it.
183. Capture plate only when angle/distance/time criteria support it.
184. Timestamp captures.
185. Distinguish disabling camera before capture from destroying it after a capture was stored.
186. Add abandoned getaway vehicle as evidence source.
187. Add weapon-class/shell-event abstractions only as optional simulated evidence; do not overpromise forensics.
188. Add evidence-confidence fusion without automatic 100% identification.
189. **Exit criteria:** a masked robbery can still produce strong vehicle/plate evidence, while properly avoiding cameras changes the case outcome.

---

# Stage 11 — Duffel bag, loot extraction and recovery

**Goal:** money exists physically enough to create risk and capacity decisions.

190. Implement `LootDirector` and `LootContainer` state.
191. Add bag tiers: none, small, standard and large.
192. Give each tier capacity by value/weight units.
193. Make larger bags allow more cash/valuables to be removed before target sources are exhausted or bag fills.
194. Add bag acquire/equip/unequip state.
195. Add visual GTA-compatible duffel prop/component where feasible.
196. Attach bag safely to player and recover from ragdoll/death/vehicle transitions.
197. Add loot-source interaction for registers, bundles, safes, vault carts/tables and valuables.
198. Use GTA pickup/grab/carry-style animation orchestration with fallback if clips are unavailable.
199. Make more loot take longer to collect.
200. Add optional movement penalty only after testing for fun and animation compatibility.
201. Add HUD capacity/value display.
202. Loot states: target source → carried bag → dropped → vehicle storage → stash → secured / seized / lost.
203. Drop the bag if design conditions require it (death, explicit drop, certain arrest states).
204. Spawn/recover a nearby bag prop while in streaming range.
205. Abstract dropped loot to a persisted record if temporarily out of range.
206. Allow player to return and recover before police/cleanup seizes it.
207. Allow police to mark unattended crime-scene loot as seized.
208. Do not credit major loot to bank balance until secured.
209. **Exit criteria:** two players using different bag tiers can leave the same robbery with different maximum takes, and losing the bag can cost the proceeds.

---

# Stage 12 — Vehicle ownership, garages, trunk inventory and meaningful cars

**Goal:** make cars persistent possessions and part of crime planning.

210. Add ownership acquisition/purchase pipeline.
211. Save owned vehicle location/home garage and reconstruction data.
212. Add garage capacity.
213. Add spawn/recovery rules so duplicate personal cars are not created accidentally.
214. Add damage/repair state at the project abstraction level.
215. Add insurance/restoration rules.
216. Add impounded state.
217. Add garage services: repair, paint, plate change and later storage upgrades.
218. Add trunk interaction zone using boot/trunk bone/door information.
219. Open trunk using vehicle door natives.
220. Play reach/store/retrieve animation where safe.
221. Implement `VehicleInventory`.
222. Store weapons, ammo, armor, masks, duffels and robbery equipment.
223. Add capacity per vehicle class if desired after balancing.
224. Keep trunk inventory with an impounded vehicle.
225. Do not automatically recreate illegal trunk items when an insurance replacement is issued unless balance explicitly allows it.
226. Add optional visual weapon props later; inventory correctness does not depend on them.
227. **Exit criteria:** choosing an owned car changes what equipment is available, and losing/impounding that car matters.

---

# Stage 13 — Limited carried weapon system

**Goal:** end the magical 30-gun pocket and make loadouts meaningful.

228. Define configurable on-person slots: sidearm, primary, optional secondary/large slot, throwable/equipment.
229. Detect the player's current GTA weapon inventory.
230. Migrate disallowed extras to safehouse/trunk storage rather than deleting them where possible.
231. Prevent reselecting weapons not currently carried.
232. Add quick trunk/safehouse loadout exchange.
233. Save carried loadout by player character.
234. Restore safely after death/arrest/mission suspension.
235. Exempt or suspend limitations during incompatible Rockstar missions where required.
236. Add arrest confiscation/seizure rules for weapons involved in serious crimes.
237. **Exit criteria:** vehicle/safehouse storage genuinely replaces the unlimited-pocket arsenal during free-roam Crime Overhaul gameplay.

---

# Stage 14 — Economy and reasons to commit crime

**Goal:** make robbery profits meaningful.

238. Implement project economy ledger layered carefully onto GTA money where appropriate.
239. Define clean cash, carried/unsecured loot and optional hot/marked proceeds.
240. Small store cash can become spendable quickly after safe escape.
241. Large bank proceeds can require securing/processing before all value becomes spendable.
242. Add owned-car purchase costs.
243. Add compatible DLC/Online vehicle catalog only for models present in the installed game build.
244. Add garage/safehouse costs/upgrades.
245. Add weapon/ammo/armor costs.
246. Add mask/disguise/clothing costs where relevant.
247. Add duffel/bag upgrade costs.
248. Add repair/repaint/license-plate service costs.
249. Add impound/legal/fine costs.
250. Add robbery equipment/preparation costs.
251. Add intelligence/crew-assistance costs in later heist tiers.
252. Balance store payouts below bank payouts and major scores without making early crime useless.
253. Add anti-farming diminishing returns/security escalation rather than arbitrary invisible cooldowns alone.
254. **Exit criteria:** expensive vehicles/equipment/property create a clear reason to earn criminal money beyond watching the cash number grow.

---

# Stage 15 — Scale convenience stores and small commercial targets

**Goal:** reuse systems, not duplicate scripts.

255. Move prototype 24/7 coordinates/content fully into data definitions.
256. Add all supported 24/7 locations.
257. Add liquor stores/gas-station style targets.
258. Validate clerk anchors, doors, registers, safe and customer zones per location.
259. Add location-specific cash/security profiles.
260. Add small offices/pawn/cash-business targets where interiors/assets support them.
261. Add ATM/safe/armored-cash events only after core robbery/law systems remain stable.
262. Ensure every target feeds the same case/witness/police/economy systems.
263. **Exit criteria:** adding a new store requires data + a small target adapter, not a new law simulation.

---

# Stage 16 — Fleeca bank vertical slice

**Goal:** first systemic free-roam bank robbery.

264. Validate a Fleeca interior on Legacy and Enhanced.
265. Define bank entrances/exits, teller positions, customer zones, guard anchors, cameras, vault/door entities and loot sources.
266. Allow normal civilian bank entry without starting a heist.
267. Add casing discoveries: entrance, teller layout, cameras, guard, rear exit, vault access/alarm/security points.
268. Persist discovered casing information.
269. Add quick teller robbery route.
270. Add hostage/crowd intimidation behavior with bounded AI.
271. Add teller compliance/defiance/panic behavior.
272. Add guard reaction.
273. Add bank alarm/dispatch severity.
274. Add vault-access route using validated door/safe interactions.
275. Add equipment requirements if needed for higher-tier vault access.
276. Populate cash bundles/valuables as loot sources.
277. Apply duffel capacity and collection-time rules.
278. Allow choosing to leave early with partial loot.
279. Feed every witness/camera/vehicle observation into normal cases.
280. Escalate police response appropriately for bank robbery/shots/hostages.
281. Preserve free-form escape; no mandatory cinematic route.
282. Secure/store/process proceeds after escape.
283. Add cooldown/security aftermath that makes immediate identical rerobbery unattractive.
284. **Exit criteria:** a Fleeca can be cased, robbed in more than one way, escaped from and investigated by the same systemic world logic as the 24/7.

---

# Stage 17 — Pacific Standard and major-score tier

**Goal:** scale complexity without abandoning systemic rules.

285. Validate Pacific Standard interior/doors/streaming by build.
286. Define multiple teller/civilian/guard zones and larger vault loot sources.
287. Add stronger security/camera configuration.
288. Add optional multiple-entry/escape approaches if geography supports them.
289. Add more severe dispatch and tactical escalation.
290. Keep partial-loot/early-exit decisions.
291. Keep bag capacity meaningful even at high payout.
292. Add crew-assistance framework only where it improves single-player behavior without turning the system into a rigid mission.
293. Add major-score target only after Pacific Standard is stable.
294. Validate Union Depository-style interiors/doors/encounter constraints before promising a specific implementation.
295. Add rare, expensive preparation requirements for major scores.
296. Add very high evidence/police consequences.
297. Add long recovery/cooldown/security response so major scores cannot be farmed every few minutes.
298. **Exit criteria:** high-tier robbery feels like the same world simulation at larger scale, not a separate scripted minigame.

---

# Stage 18 — Presentation, UI, dialogue and audio polish

**Goal:** make systems feel like part of GTA rather than debug scripts.

299. Replace debug-only robbery text with minimal player HUD.
300. Add original Rockstar-inspired visual language without copying GTA VI proprietary UI assets.
301. Add duffel value/capacity element.
302. Add witness/report alert.
303. Add investigation/search status.
304. Add active person warrant and vehicle BOLO summaries when useful.
305. Add garage plate-change confirmation and cost UI.
306. Add trunk inventory UI.
307. Add vehicle catalog/purchase UI.
308. Build data-driven dialogue files: clerks, civilians, witnesses, police, guards and hostages.
309. Add semantic tags such as robbery begin, compliant, defiant, alarm triggered, repeat robber recognized, face seen/not seen, vehicle/plate seen and witness panic.
310. Map tags to validated existing ambient lines when suitable.
311. Add subtitle fallback for every semantic event.
312. Add project-owned original/authorized voice packs optionally.
313. Never require cloned Rockstar actor voices.
314. Add police radio flavor only where it can accurately represent case information.
315. Add animation blending/timing polish around bag, trunk, register, safe, surrender and interview interactions.
316. **Exit criteria:** normal play exposes clean in-world feedback; debug overlays can be completely disabled.

---

# Stage 19 — Compatibility, robustness and performance pass

**Goal:** survive real GTA sessions, not just controlled tests.

317. Validate current Script Hook V Legacy build.
318. Validate current Script Hook V Enhanced build.
319. Test story character swaps.
320. Test save/load during dormant and active cases.
321. Test game death during robbery.
322. Test arrest during robbery.
323. Test mission start while systems are active.
324. Test cutscene transitions.
325. Test interior transitions.
326. Test fast travel/teleport from trainers as a robustness case.
327. Test entity cleanup/despawn at every state-machine stage.
328. Test destroyed getaway vehicle.
329. Test changed plate and paint after a crime.
330. Test dropped/seized/recovered duffel.
331. Test killed/replaced clerk.
332. Test multiple simultaneous cases.
333. Test repeated robberies over long saves.
334. Profile witness scanning and raycasts.
335. Profile police planning.
336. Cap active high-detail scenes.
337. Abstract distant scenes/cases.
338. Add save migration tests.
339. Add diagnostic report generator for bug submissions.
340. **Exit criteria:** multi-hour Story Mode play remains stable without runaway peds, duplicated vehicles, corrupted saves or high constant CPU usage.

---

# Stage 20 — Release engineering

341. Freeze schema for first public alpha.
342. Produce signed/versioned release archive where signing infrastructure exists.
343. Package `GTA_Crime_Overhaul.asi`.
344. Package config/data files.
345. Package install/readme/changelog.
346. Do **not** package Script Hook V runtime/ASI loader.
347. Add exact supported game/Script Hook V versions to release notes.
348. Add backup instructions.
349. Add upgrade/save migration instructions.
350. Publish alpha focused on one-store/full-law vertical slice before claiming full overhaul.
351. Expand beta only when stores, persistence, warrants, vehicles and duffel loop are stable.
352. Release 1.0 only after Fleeca/Pacific Standard, economy, vehicle ownership, trunk weapons, plate changes and persistent law are integrated.

---

# Final gameplay acceptance scenarios

The project is not complete until these work end-to-end:

### Scenario A — clean masked store robbery

353. Buy/equip a mask and duffel.
354. Arrive in an owned car.
355. Rob a clerk while face remains unseen.
356. A witness sees clothing and car but not plate.
357. Escape before police arrive.
358. Police investigate the store and build an unknown-suspect case.
359. Change clothes and switch vehicles without being observed.
360. No magical identification occurs.

### Scenario B — careless unmasked repeat robber

361. Rob a store unmasked.
362. Clerk gets strong face ID and car/plate data.
363. Escape immediate wanted response.
364. Persistent warrant/vehicle BOLO remains.
365. Return days later.
366. Same logical clerk recognizes the player and silently reports them.

### Scenario C — plate countermeasure

367. Commit robbery in an owned vehicle with plate `ABC123`.
368. Case stores `ABC123` as historical evidence.
369. Lose the search and reach a garage.
370. Pay to change plate to `XYZ789`.
371. Vehicle record/current GTA entity now display `XYZ789`.
372. Old case still correctly says the witnessed plate was `ABC123`.
373. Direct old-plate camera/patrol matching is reduced, but a previously proven owner/vehicle link may still matter.

### Scenario D — duffel capacity

374. Attempt a bank with a small bag and leave when full.
375. Repeat later with a large bag.
376. Large bag permits more loot but takes longer to fill and creates greater loss risk.
377. Drop the loaded bag during escape.
378. Police may seize it if the player cannot recover it.

### Scenario E — meaningful personal car/loadout

379. Store a rifle, armor, large duffel and robbery equipment in an owned vehicle.
380. Choose that car for a bank job and retrieve equipment from the trunk.
381. Police impound the getaway car after arrest.
382. The vehicle and its stored equipment remain consequences rather than teleporting safely home.

### Scenario F — systemic Fleeca robbery

383. Enter bank normally and case it.
384. Return prepared and initiate robbery through player action, not a giant mission marker.
385. Control teller/crowd/guard situation.
386. Access available cash/vault route.
387. Physically fill bag until player chooses to leave or capacity is reached.
388. Witness/camera data determines suspect and vehicle description.
389. Police response/search uses last-known data.
390. Escape, secure loot, manage warrant/vehicle evidence and spend proceeds on meaningful progression.

**That end-to-end loop is the target.**
