// 3D chess board built with Three.js. Procedural piece geometry (no GLB).
// Synced from FEN via setBoardFromFen(); click events forwarded via callback.

import * as THREE from './vendor/three.module.js';
import { OrbitControls } from './vendor/OrbitControls.js';

const FILES = ['a','b','c','d','e','f','g','h'];

const COLORS = {
    bg:               0x1e1e1e,
    boardBase:        0x2a1f15,
    light:            0xf0d9b5,
    dark:             0xb58863,
    whitePiece:       0xeeeae0,
    blackPiece:       0x252525,
    highlightFrom:    0xffe066,
    highlightTo:      0xfff299,
    highlightSelected:0x4a90e2,
    highlightCheck:   0xff4040,
    legalDot:         0x4caf50,
};

export class Board3D {
    constructor(container, callbacks = {}) {
        this.container  = container;
        this.callbacks  = callbacks;
        this.flipped    = false;
        this.pieces     = new Map(); // square -> Group
        this._pickables = [];        // squares + piece root meshes for raycast
        this._highlightObjs = [];
        this._activeAnim = null;     // active move animation (or null)
        this._init();
    }

    _init() {
        this.scene = new THREE.Scene();
        this.scene.background = new THREE.Color(COLORS.bg);

        this.renderer = new THREE.WebGLRenderer({ antialias: true });
        this.renderer.shadowMap.enabled = true;
        this.renderer.shadowMap.type = THREE.PCFSoftShadowMap;
        this.renderer.outputColorSpace = THREE.SRGBColorSpace;
        this.container.appendChild(this.renderer.domElement);

        // FoV 42° + camera farther back so a 8-unit board fits horizontally
        // even when window aspect is square.
        this.camera = new THREE.PerspectiveCamera(42, 1, 0.1, 100);
        this._setCamera();

        // OrbitControls: drag = rotate, wheel = zoom. Pan disabled to keep the
        // board centred. Polar angle clamped so user can't dive below the board.
        this.controls = new OrbitControls(this.camera, this.renderer.domElement);
        this.controls.target.set(0, 0, 0);
        this.controls.enableDamping = true;
        this.controls.dampingFactor = 0.08;
        this.controls.enablePan = false;
        this.controls.minDistance = 6;
        this.controls.maxDistance = 22;
        this.controls.minPolarAngle = 0.15;
        this.controls.maxPolarAngle = Math.PI / 2 - 0.05;
        this.controls.update();

        // Lights
        this.scene.add(new THREE.AmbientLight(0xffffff, 0.55));
        const dir = new THREE.DirectionalLight(0xffffff, 0.95);
        dir.position.set(4, 10, 6);
        dir.castShadow = true;
        dir.shadow.mapSize.set(1024, 1024);
        const c = dir.shadow.camera;
        c.left = -7; c.right = 7; c.top = 7; c.bottom = -7;
        c.near = 1; c.far = 25;
        this.scene.add(dir);
        // Fill light from opposite side, no shadow
        const fill = new THREE.DirectionalLight(0xa0c4ff, 0.25);
        fill.position.set(-5, 6, -4);
        this.scene.add(fill);

        // Board base (border around the 8x8 squares)
        const baseGeom = new THREE.BoxGeometry(9.4, 0.4, 9.4);
        const baseMat = new THREE.MeshStandardMaterial({
            color: COLORS.boardBase, roughness: 0.85, metalness: 0.05,
        });
        const base = new THREE.Mesh(baseGeom, baseMat);
        base.position.y = -0.2;
        base.receiveShadow = true;
        this.scene.add(base);

        // Squares
        this.squareMeshes = [];
        for (let r = 0; r < 8; r++) {
            for (let f = 0; f < 8; f++) {
                const isLight = (r + f) % 2 === 1;
                const geom = new THREE.BoxGeometry(1, 0.05, 1);
                const mat = new THREE.MeshStandardMaterial({
                    color: isLight ? COLORS.light : COLORS.dark,
                    roughness: 0.75, metalness: 0.0,
                });
                const mesh = new THREE.Mesh(geom, mat);
                mesh.position.set(f - 3.5, 0.025, 3.5 - r);
                mesh.receiveShadow = true;
                mesh.userData.square = FILES[f] + (r + 1);
                this.scene.add(mesh);
                this.squareMeshes.push(mesh);
                this._pickables.push(mesh);
            }
        }

        this.pieceGroup = new THREE.Group();
        this.scene.add(this.pieceGroup);

        this.highlightGroup = new THREE.Group();
        this.scene.add(this.highlightGroup);

        // Resize / click
        this._ro = new ResizeObserver(() => this._resize());
        this._ro.observe(this.container);
        this._resize();

        this._raycaster = new THREE.Raycaster();
        this._mouse = new THREE.Vector2();
        this._dragStart = null;
        const dom = this.renderer.domElement;
        dom.addEventListener('pointerdown', (ev) => {
            this._dragStart = { x: ev.clientX, y: ev.clientY };
        });
        dom.addEventListener('pointerup', (ev) => {
            if (!this._dragStart) return;
            const dx = ev.clientX - this._dragStart.x;
            const dy = ev.clientY - this._dragStart.y;
            this._dragStart = null;
            // Treat as a click only if pointer barely moved — otherwise OrbitControls
            // already handled the rotation.
            if (Math.hypot(dx, dy) <= 5) this._onClick(ev);
        });
        dom.addEventListener('contextmenu', (ev) => ev.preventDefault());

        this._loop = () => {
            this.controls.update();
            if (this._activeAnim) this._tickAnim(performance.now());
            this.renderer.render(this.scene, this.camera);
            this._raf = requestAnimationFrame(this._loop);
        };
        this._loop();
    }

    _setCamera() {
        // Higher distance + lower y → less steep angle, full board visible.
        // White at +Z (rank 1). Flipped → camera on -Z side for black player.
        const z = this.flipped ? -10 : 10;
        this.camera.position.set(0, 8, z);
        this.camera.lookAt(0, 0, 0);
        if (this.controls) {
            this.controls.target.set(0, 0, 0);
            this.controls.update();
        }
    }

    setFlipped(flipped) {
        this.flipped = !!flipped;
        this._setCamera();
    }

    resetView() {
        this._setCamera();
    }

    _resize() {
        const w = this.container.clientWidth;
        const h = this.container.clientHeight;
        if (!w || !h) return;
        this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
        this.renderer.setSize(w, h, false);
        this.camera.aspect = w / h;
        this.camera.updateProjectionMatrix();
    }

    _onClick(ev) {
        const rect = this.renderer.domElement.getBoundingClientRect();
        this._mouse.x = ((ev.clientX - rect.left) / rect.width) * 2 - 1;
        this._mouse.y = -((ev.clientY - rect.top) / rect.height) * 2 + 1;
        this._raycaster.setFromCamera(this._mouse, this.camera);
        const hits = this._raycaster.intersectObjects(this._pickables, true);
        if (!hits.length) return;
        let obj = hits[0].object;
        let sq = obj.userData?.square;
        while (!sq && obj.parent) { obj = obj.parent; sq = obj.userData?.square; }
        if (sq && this.callbacks.onSquareClick) this.callbacks.onSquareClick(sq);
    }

    _squareToWorld(sq) {
        const file = FILES.indexOf(sq[0]);
        const rank = parseInt(sq[1], 10) - 1;
        return new THREE.Vector3(file - 3.5, 0.05, 3.5 - rank);
    }

    setBoardFromFen(fen) {
        // Drop any in-progress animation; the upcoming clearPieces would orphan
        // its piece refs anyway.
        this._activeAnim = null;
        this._clearPieces();
        const placement = fen.split(' ')[0];
        let rank = 7, file = 0;
        for (const c of placement) {
            if (c === '/') { rank--; file = 0; continue; }
            if (/\d/.test(c)) { file += parseInt(c, 10); continue; }
            const isWhite = c === c.toUpperCase();
            const type = c.toLowerCase();
            this._placePiece(FILES[file] + (rank + 1), type, isWhite ? 'w' : 'b');
            file++;
        }
    }

    _clearPieces() {
        // Remove pickable references for old pieces
        this._pickables = this._pickables.filter(p => !p.userData.isPiece);
        while (this.pieceGroup.children.length) {
            const child = this.pieceGroup.children.pop();
            this._disposeObject(child);
        }
        this.pieces.clear();
    }

    _placePiece(square, type, color) {
        const piece = createPiece(type, color);
        const pos = this._squareToWorld(square);
        piece.position.copy(pos);
        // Knights face toward the opponent
        if (type === 'n') piece.rotation.y = (color === 'w') ? Math.PI : 0;
        piece.userData.square = square;
        piece.userData.isPiece = true;
        // Mark all child meshes pickable (raycaster needs leaf meshes; we walk up to find square)
        piece.traverse(o => {
            if (o.isMesh) {
                o.userData.isPiece = true;
                this._pickables.push(o);
            }
        });
        this.pieceGroup.add(piece);
        this.pieces.set(square, piece);
    }

    setHighlights({ from, to, selected, legalSquares, checkSquare } = {}) {
        // Clear existing
        while (this.highlightGroup.children.length) {
            const c = this.highlightGroup.children.pop();
            this._disposeObject(c);
        }

        const addPlane = (sq, color, alpha = 0.55, h = 0.06) => {
            if (!sq) return;
            const geom = new THREE.PlaneGeometry(0.96, 0.96);
            const mat = new THREE.MeshBasicMaterial({
                color, transparent: true, opacity: alpha, depthWrite: false,
            });
            const mesh = new THREE.Mesh(geom, mat);
            mesh.rotation.x = -Math.PI / 2;
            const pos = this._squareToWorld(sq);
            mesh.position.set(pos.x, h, pos.z);
            this.highlightGroup.add(mesh);
        };

        addPlane(from, COLORS.highlightFrom, 0.50, 0.058);
        addPlane(to,   COLORS.highlightTo,   0.45, 0.060);
        addPlane(selected, COLORS.highlightSelected, 0.55, 0.062);
        addPlane(checkSquare, COLORS.highlightCheck, 0.65, 0.064);

        if (legalSquares?.length) {
            for (const sq of legalSquares) {
                const isCapture = this.pieces.has(sq);
                const geom = isCapture
                    ? new THREE.RingGeometry(0.42, 0.48, 32)
                    : new THREE.CircleGeometry(0.16, 24);
                const mat = new THREE.MeshBasicMaterial({
                    color: COLORS.legalDot, transparent: true, opacity: 0.65, depthWrite: false,
                });
                const mesh = new THREE.Mesh(geom, mat);
                mesh.rotation.x = -Math.PI / 2;
                const pos = this._squareToWorld(sq);
                mesh.position.set(pos.x, 0.066, pos.z);
                this.highlightGroup.add(mesh);
            }
        }
    }

    // Animate a chess.js verbose move: slide the moving piece, fade-out captured
    // piece, also relocate rook on castling and replace pawn on promotion.
    // Returns true if the animation was scheduled, false if state didn't match
    // (caller should fall back to setBoardFromFen).
    animateMove(move, opts = {}) {
        if (!move || !move.from || !move.to) return false;
        // Snap any in-progress animation to its final state first.
        this._finishAnimations();

        const piece = this.pieces.get(move.from);
        if (!piece) return false;

        const duration = opts.duration ?? 280;
        const animations = [];
        const isKnight = move.piece === 'n';

        // Captured piece (regular capture or en passant)
        if (move.captured) {
            let capSq = move.to;
            const flags = move.flags || '';
            if (flags.includes('e')) {
                const capRank = move.color === 'w'
                    ? (parseInt(move.to[1], 10) - 1)
                    : (parseInt(move.to[1], 10) + 1);
                capSq = move.to[0] + capRank;
            }
            const captured = this.pieces.get(capSq);
            if (captured) {
                this.pieces.delete(capSq);
                this._removePiecePickables(captured);
                animations.push({
                    type: 'fade',
                    obj: captured,
                    start: 0.45, end: 1.0,
                    onDone: () => this._disposeObject(captured),
                });
            }
        }

        // Moving piece
        animations.push({
            type: 'move',
            obj: piece,
            startPos: piece.position.clone(),
            endPos: this._squareToWorld(move.to),
            arc: isKnight ? 0.55 : 0.08,
            start: 0.0, end: 1.0,
        });

        this.pieces.delete(move.from);
        this.pieces.set(move.to, piece);
        piece.userData.square = move.to;
        piece.traverse(o => { if (o.userData && o.userData.isPiece) o.userData.square = move.to; });

        // Castling: also slide the rook
        const flags = move.flags || '';
        if (flags.includes('k') || flags.includes('q')) {
            const rank = move.color === 'w' ? '1' : '8';
            const isKingside = flags.includes('k');
            const rookFrom = (isKingside ? 'h' : 'a') + rank;
            const rookTo   = (isKingside ? 'f' : 'd') + rank;
            const rook = this.pieces.get(rookFrom);
            if (rook) {
                animations.push({
                    type: 'move',
                    obj: rook,
                    startPos: rook.position.clone(),
                    endPos: this._squareToWorld(rookTo),
                    arc: 0.05,
                    start: 0.0, end: 1.0,
                });
                this.pieces.delete(rookFrom);
                this.pieces.set(rookTo, rook);
                rook.userData.square = rookTo;
                rook.traverse(o => { if (o.userData && o.userData.isPiece) o.userData.square = rookTo; });
            }
        }

        // Promotion: replace pawn with promoted piece when animation ends
        let promoCallback = null;
        if (move.promotion) {
            promoCallback = () => {
                this._removePiecePickables(piece);
                this._disposeObject(piece);
                this.pieces.delete(move.to);
                this._placePiece(move.to, move.promotion, move.color);
            };
        }

        this._activeAnim = {
            startTime: performance.now(),
            duration,
            animations,
            onComplete: promoCallback,
        };
        return true;
    }

    _tickAnim(now) {
        const a = this._activeAnim;
        if (!a) return;
        let t = (now - a.startTime) / a.duration;
        if (t > 1) t = 1;
        // easeInOutQuad
        const ease = (x) => x < 0.5 ? 2 * x * x : 1 - Math.pow(-2 * x + 2, 2) / 2;

        for (const sub of a.animations) {
            if (sub.type === 'move') {
                const span = sub.end - sub.start;
                let lt = span > 0 ? (t - sub.start) / span : 1;
                if (lt < 0) lt = 0; else if (lt > 1) lt = 1;
                const e = ease(lt);
                sub.obj.position.x = sub.startPos.x + (sub.endPos.x - sub.startPos.x) * e;
                sub.obj.position.z = sub.startPos.z + (sub.endPos.z - sub.startPos.z) * e;
                const arcH = sub.arc * Math.sin(lt * Math.PI);
                sub.obj.position.y = sub.startPos.y + (sub.endPos.y - sub.startPos.y) * e + arcH;
            } else if (sub.type === 'fade') {
                const span = sub.end - sub.start;
                let lt = span > 0 ? (t - sub.start) / span : 1;
                if (lt < 0) lt = 0; else if (lt > 1) lt = 1;
                const op = 1 - lt;
                sub.obj.traverse(o => {
                    if (!o.material) return;
                    const apply = (m) => { m.transparent = true; m.opacity = op; m.depthWrite = false; };
                    if (Array.isArray(o.material)) o.material.forEach(apply);
                    else apply(o.material);
                });
            }
        }

        if (t >= 1) {
            for (const sub of a.animations) {
                if (sub.type === 'fade' && sub.onDone) sub.onDone();
            }
            const cb = a.onComplete;
            this._activeAnim = null;
            if (cb) cb();
        }
    }

    _finishAnimations() {
        const a = this._activeAnim;
        if (!a) return;
        for (const sub of a.animations) {
            if (sub.type === 'move') sub.obj.position.copy(sub.endPos);
            else if (sub.type === 'fade' && sub.onDone) sub.onDone();
        }
        const cb = a.onComplete;
        this._activeAnim = null;
        if (cb) cb();
    }

    _removePiecePickables(piece) {
        const meshes = new Set();
        piece.traverse(o => { if (o.isMesh) meshes.add(o); });
        this._pickables = this._pickables.filter(p => !meshes.has(p));
    }

    pause() {
        if (this._raf != null) { cancelAnimationFrame(this._raf); this._raf = null; }
    }
    resume() {
        if (this._raf == null) this._loop();
    }

    _disposeObject(obj) {
        obj.traverse?.(o => {
            if (o.geometry) o.geometry.dispose?.();
            if (o.material) {
                if (Array.isArray(o.material)) o.material.forEach(m => m.dispose?.());
                else o.material.dispose?.();
            }
        });
        obj.parent?.remove(obj);
    }

    dispose() {
        this.pause();
        this._ro?.disconnect();
        this._disposeObject(this.scene);
        this.renderer.dispose();
        this.renderer.domElement.remove();
    }
}

// ── Piece geometry (procedural) ─────────────────────────────────────

function pieceMaterial(color) {
    return new THREE.MeshStandardMaterial({
        color: color === 'w' ? COLORS.whitePiece : COLORS.blackPiece,
        roughness: 0.45,
        metalness: 0.18,
    });
}

function makeLathe(profilePts, color) {
    const pts = profilePts.map(([r, y]) => new THREE.Vector2(r, y));
    const geom = new THREE.LatheGeometry(pts, 36);
    const mesh = new THREE.Mesh(geom, pieceMaterial(color));
    mesh.castShadow = true;
    mesh.receiveShadow = true;
    return mesh;
}

function createPawn(color) {
    const profile = [
        [0.00, 0.00],
        [0.34, 0.00],
        [0.34, 0.07],
        [0.40, 0.07],
        [0.40, 0.11],
        [0.30, 0.13],
        [0.18, 0.20],
        [0.16, 0.42],
        [0.21, 0.46],
        [0.21, 0.52],
        [0.00, 0.52],
    ];
    const g = new THREE.Group();
    g.add(makeLathe(profile, color));
    const head = new THREE.Mesh(
        new THREE.SphereGeometry(0.19, 28, 18),
        pieceMaterial(color),
    );
    head.position.y = 0.62;
    head.castShadow = true;
    g.add(head);
    return g;
}

function createRook(color) {
    const profile = [
        [0.00, 0.00],
        [0.38, 0.00],
        [0.38, 0.07],
        [0.44, 0.07],
        [0.44, 0.12],
        [0.32, 0.14],
        [0.26, 0.20],
        [0.24, 0.62],
        [0.34, 0.62],
        [0.34, 0.70],
        [0.00, 0.70],
    ];
    const g = new THREE.Group();
    g.add(makeLathe(profile, color));
    const crennGeom = new THREE.BoxGeometry(0.13, 0.13, 0.13);
    const mat = pieceMaterial(color);
    const positions = [
        [ 0.22, 0.78,  0.00], [-0.22, 0.78,  0.00],
        [ 0.00, 0.78,  0.22], [ 0.00, 0.78, -0.22],
    ];
    for (const [x, y, z] of positions) {
        const m = new THREE.Mesh(crennGeom, mat);
        m.position.set(x, y, z);
        m.castShadow = true;
        g.add(m);
    }
    return g;
}

function createBishop(color) {
    const profile = [
        [0.00, 0.00],
        [0.36, 0.00],
        [0.36, 0.07],
        [0.42, 0.07],
        [0.42, 0.11],
        [0.30, 0.14],
        [0.20, 0.22],
        [0.18, 0.55],
        [0.26, 0.66],
        [0.10, 0.85],
        [0.00, 0.96],
    ];
    const g = new THREE.Group();
    g.add(makeLathe(profile, color));
    const top = new THREE.Mesh(
        new THREE.SphereGeometry(0.075, 18, 14),
        pieceMaterial(color),
    );
    top.position.y = 1.02;
    top.castShadow = true;
    g.add(top);
    return g;
}

function createQueen(color) {
    const profile = [
        [0.00, 0.00],
        [0.40, 0.00],
        [0.40, 0.08],
        [0.46, 0.08],
        [0.46, 0.12],
        [0.34, 0.15],
        [0.24, 0.24],
        [0.22, 0.72],
        [0.32, 0.80],
        [0.36, 0.86],
        [0.00, 0.86],
    ];
    const g = new THREE.Group();
    g.add(makeLathe(profile, color));
    const mat = pieceMaterial(color);
    const ballGeom = new THREE.SphereGeometry(0.062, 16, 12);
    const N = 8;
    for (let i = 0; i < N; i++) {
        const a = (i / N) * Math.PI * 2;
        const m = new THREE.Mesh(ballGeom, mat);
        m.position.set(Math.cos(a) * 0.30, 0.92, Math.sin(a) * 0.30);
        m.castShadow = true;
        g.add(m);
    }
    const top = new THREE.Mesh(new THREE.SphereGeometry(0.10, 18, 14), mat);
    top.position.y = 0.98;
    top.castShadow = true;
    g.add(top);
    return g;
}

function createKing(color) {
    const profile = [
        [0.00, 0.00],
        [0.40, 0.00],
        [0.40, 0.08],
        [0.46, 0.08],
        [0.46, 0.12],
        [0.34, 0.15],
        [0.26, 0.24],
        [0.24, 0.80],
        [0.34, 0.88],
        [0.36, 0.94],
        [0.00, 0.94],
    ];
    const g = new THREE.Group();
    g.add(makeLathe(profile, color));
    const mat = pieceMaterial(color);
    const v = new THREE.Mesh(new THREE.BoxGeometry(0.06, 0.24, 0.06), mat);
    v.position.y = 1.08;
    v.castShadow = true;
    const h = new THREE.Mesh(new THREE.BoxGeometry(0.18, 0.06, 0.06), mat);
    h.position.y = 1.06;
    h.castShadow = true;
    g.add(v); g.add(h);
    return g;
}

function createKnight(color) {
    // Asymmetric — composed from boxes/cones over a lathed base.
    const g = new THREE.Group();
    const mat = pieceMaterial(color);

    g.add(makeLathe([
        [0.00, 0.00],
        [0.38, 0.00],
        [0.38, 0.08],
        [0.44, 0.08],
        [0.44, 0.12],
        [0.30, 0.16],
        [0.00, 0.16],
    ], color));

    const body = new THREE.Mesh(new THREE.BoxGeometry(0.32, 0.42, 0.42), mat);
    body.position.set(0, 0.36, -0.04);
    body.castShadow = true;
    g.add(body);

    const head = new THREE.Mesh(new THREE.BoxGeometry(0.26, 0.22, 0.48), mat);
    head.position.set(0, 0.66, 0.06);
    head.rotation.x = -0.30;
    head.castShadow = true;
    g.add(head);

    const snout = new THREE.Mesh(new THREE.BoxGeometry(0.20, 0.14, 0.18), mat);
    snout.position.set(0, 0.56, 0.30);
    snout.castShadow = true;
    g.add(snout);

    const earGeom = new THREE.ConeGeometry(0.05, 0.13, 10);
    const earL = new THREE.Mesh(earGeom, mat);
    earL.position.set( 0.08, 0.84, -0.05);
    earL.castShadow = true;
    const earR = earL.clone();
    earR.position.x = -0.08;
    g.add(earL); g.add(earR);

    return g;
}

function createPiece(type, color) {
    switch (type) {
        case 'p': return createPawn(color);
        case 'r': return createRook(color);
        case 'n': return createKnight(color);
        case 'b': return createBishop(color);
        case 'q': return createQueen(color);
        case 'k': return createKing(color);
    }
    return new THREE.Group();
}
