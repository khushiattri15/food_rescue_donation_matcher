/*
 * Food Rescue & Donation Matcher
 * Course : Data Structures Lab (15B17CI371), JIIT Noida
 * Team   : Khushi Attri, Shreya Sharma, Mayank Sethi
 *
 * Data structures used (as per synopsis):
 *   1. struct          -> Profile, Donation, Request, Match
 *   2. Min-Heap        -> donations ordered by expiry, requests by urgency
 *   3. Grid index      -> proximity lookup of donations near a request
 *   4. Queue (linked)  -> intake pipeline for donations and requests
 *   5. Hash map        -> profile lookup by ID (separate chaining)
 *   6. Linked list     -> match history
 *   7. Dynamic arrays  -> vectors for storage
 *   8. File handling   -> persistence between sessions
 *   9. Graph           -> hub road network: adjacency matrix + list, BFS, DFS,
 *                         Prim's MST, Kruskal's MST (CityGraph class)
 *
 * Compile: g++ -std=c++11 -o foodrescue food_rescue_matcher.cpp
 */
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
using namespace std;

const int GRID_SIZE = 10;      // 10 x 10 cells
const double CELL_KM = 10.0;   // each cell is 10 km wide (city is 100 x 100 km)
const double MAX_RADIUS = 15.0;// max distance (km) for a donor-recipient match
const int MIN_SHELF_HOURS = 1; // food must have at least this many hours left

/* ---------------------------------------------------------------
 *  Helper functions
 * ------------------------------------------------------------- */
string toLower(string s) {
    for (size_t i = 0; i < s.size(); i++) s[i] = tolower(s[i]);
    return s;
}

int readInt(const string &prompt, int lo, int hi) {
    int v;
    while (true) {
        cout << prompt;
        if (cin >> v && v >= lo && v <= hi) { cin.ignore(1000, '\n'); return v; }
        if (cin.eof()) { cout << "\nInput ended. Exiting.\n"; exit(0); }
        cin.clear();
        cin.ignore(1000, '\n');
        cout << "  Invalid input. Enter a number between " << lo << " and " << hi << ".\n";
    }
}

string readLine(const string &prompt) {
    string s;
    cout << prompt;
    getline(cin, s);
    return s;
}

vector<string> split(const string &line, char d) {
    vector<string> out;
    string tok;
    stringstream ss(line);
    while (getline(ss, tok, d)) out.push_back(tok);
    return out;
}

/* ---------------------------------------------------------------
 *  Record structures
 * ------------------------------------------------------------- */
struct Profile {
    string id, name, phone;
    char type;            // 'D' = donor, 'R' = recipient
    double x, y;          // location on the city grid (km)
    bool active;
    bool complete() const { return active && !name.empty() && !phone.empty() && phone != "-"; }
};

struct Donation {
    int id;
    string donorId, food;
    int qty, remaining;
    int expiryTime;       // absolute clock hour at which it spoils
    double x, y;
    string status;        // QUEUED, OPEN, MATCHED, EXPIRED
};

struct Request {
    int id;
    string recipId, food;
    int qty, remaining;
    int urgency;          // 1 = most urgent ... 5 = least urgent
    double x, y;
    string status;        // QUEUED, OPEN, FULFILLED
};

struct Match {
    int id, donationId, requestId;
    string donorId, recipId, food;
    int qty;
    double dist;
    int time;
};

/* ---------------------------------------------------------------
 *  Hash map: profile ID -> Profile (separate chaining)
 * ------------------------------------------------------------- */
class ProfileMap {
    struct Node { Profile data; Node *next; };
    static const int BUCKETS = 53;
    Node *table[BUCKETS];
    int count;

    int hashFn(const string &key) const {      // djb2 hash
        unsigned long h = 5381;
        for (size_t i = 0; i < key.size(); i++) h = h * 33 + key[i];
        return h % BUCKETS;
    }
public:
    ProfileMap() : count(0) { for (int i = 0; i < BUCKETS; i++) table[i] = NULL; }
    ~ProfileMap() {
        for (int i = 0; i < BUCKETS; i++) {
            Node *n = table[i];
            while (n) { Node *t = n->next; delete n; n = t; }
        }
    }
    void put(const Profile &p) {
        int h = hashFn(p.id);
        for (Node *n = table[h]; n; n = n->next)
            if (n->data.id == p.id) { n->data = p; return; }
        Node *nn = new Node{p, table[h]};
        table[h] = nn;
        count++;
    }
    Profile *get(const string &id) {
        for (Node *n = table[hashFn(id)]; n; n = n->next)
            if (n->data.id == id) return &n->data;
        return NULL;
    }
    int size() const { return count; }
    void collect(vector<Profile> &out) const {
        for (int i = 0; i < BUCKETS; i++)
            for (Node *n = table[i]; n; n = n->next) out.push_back(n->data);
    }
};

/* ---------------------------------------------------------------
 *  Min-heap (array based). Smallest key is on top.
 *  key  = expiry time (donations) or urgency (requests)
 *  tie  = record id, so older entries win a tie (fairness)
 * ------------------------------------------------------------- */
struct HeapItem {
    int key, tie, id;
    bool operator<(const HeapItem &o) const {
        if (key != o.key) return key < o.key;
        return tie < o.tie;
    }
};

class MinHeap {
    vector<HeapItem> a;
    void siftUp(int i) {
        while (i > 0) {
            int p = (i - 1) / 2;
            if (a[i] < a[p]) { swap(a[i], a[p]); i = p; }
            else break;
        }
    }
    void siftDown(int i) {
        int n = a.size();
        while (true) {
            int l = 2 * i + 1, r = 2 * i + 2, m = i;
            if (l < n && a[l] < a[m]) m = l;
            if (r < n && a[r] < a[m]) m = r;
            if (m == i) break;
            swap(a[i], a[m]);
            i = m;
        }
    }
public:
    bool empty() const { return a.empty(); }
    int size() const { return a.size(); }
    void push(HeapItem h) { a.push_back(h); siftUp(a.size() - 1); }
    HeapItem top() const { return a[0]; }          // heap-peek
    HeapItem pop() {
        HeapItem t = a[0];
        a[0] = a.back();
        a.pop_back();
        if (!a.empty()) siftDown(0);
        return t;
    }
    void clear() { a.clear(); }
};

/* ---------------------------------------------------------------
 *  Linked-list queue (FIFO) for the intake pipeline
 * ------------------------------------------------------------- */
class IntakeQueue {
    struct Node { int id; Node *next; };
    Node *head, *tail;
    int n;
public:
    IntakeQueue() : head(NULL), tail(NULL), n(0) {}
    ~IntakeQueue() { while (!empty()) dequeue(); }
    bool empty() const { return head == NULL; }
    int size() const { return n; }
    void enqueue(int id) {
        Node *nn = new Node{id, NULL};
        if (tail) tail->next = nn; else head = nn;
        tail = nn;
        n++;
    }
    int dequeue() {
        Node *t = head;
        int id = t->id;
        head = head->next;
        if (!head) tail = NULL;
        delete t;
        n--;
        return id;
    }
};

/* ---------------------------------------------------------------
 *  Grid spatial index: each cell stores donation IDs inside it.
 *  A query only scans the cells that overlap the search radius
 *  instead of every donation.
 * ------------------------------------------------------------- */
class GridIndex {
    vector<int> cells[GRID_SIZE][GRID_SIZE];
    static int cellOf(double v) {
        int c = (int)(v / CELL_KM);
        return max(0, min(GRID_SIZE - 1, c));
    }
public:
    void insert(int donId, double x, double y) { cells[cellOf(x)][cellOf(y)].push_back(donId); }
    void remove(int donId, double x, double y) {
        vector<int> &c = cells[cellOf(x)][cellOf(y)];
        c.erase(std::remove(c.begin(), c.end(), donId), c.end());
    }
    vector<int> nearby(double x, double y, double radius) const {
        vector<int> out;
        int x0 = cellOf(x - radius), x1 = cellOf(x + radius);
        int y0 = cellOf(y - radius), y1 = cellOf(y + radius);
        for (int i = x0; i <= x1; i++)
            for (int j = y0; j <= y1; j++)
                out.insert(out.end(), cells[i][j].begin(), cells[i][j].end());
        return out;
    }
    void clear() {
        for (int i = 0; i < GRID_SIZE; i++)
            for (int j = 0; j < GRID_SIZE; j++) cells[i][j].clear();
    }
};

/* ---------------------------------------------------------------
 *  Linked list for match history (newest added at the tail)
 * ------------------------------------------------------------- */
class MatchHistory {
    struct Node { Match m; Node *next; };
    Node *head, *tail;
    int n;
public:
    MatchHistory() : head(NULL), tail(NULL), n(0) {}
    ~MatchHistory() { while (head) { Node *t = head->next; delete head; head = t; } }
    void add(const Match &m) {
        Node *nn = new Node{m, NULL};
        if (tail) tail->next = nn; else head = nn;
        tail = nn;
        n++;
    }
    int size() const { return n; }
    // owner == "" prints everything
    int print(const string &owner) const {
        int shown = 0;
        for (Node *p = head; p; p = p->next) {
            if (!owner.empty() && p->m.donorId != owner && p->m.recipId != owner) continue;
            cout << "  Match #" << p->m.id << " | " << p->m.qty << " x " << p->m.food
                 << " | " << p->m.donorId << " -> " << p->m.recipId
                 << " | " << fixed << setprecision(1) << p->m.dist << " km"
                 << " | hour " << p->m.time << "\n";
            shown++;
        }
        return shown;
    }
    void save(ofstream &f) const {
        for (Node *p = head; p; p = p->next)
            f << p->m.id << '|' << p->m.donationId << '|' << p->m.requestId << '|' << p->m.donorId
              << '|' << p->m.recipId << '|' << p->m.food << '|' << p->m.qty << '|' << p->m.dist
              << '|' << p->m.time << '\n';
    }
};

/* ---------------------------------------------------------------
 *  City road network (graph) of 8 collection hubs.
 *  Stored TWICE on purpose, to show both representations:
 *    - adjacency matrix : fast "is there a road u-v?" check
 *    - adjacency list   : used by BFS / DFS / Prim (saves space)
 *  Algorithms: BFS, DFS, Prim's MST (uses our MinHeap), Kruskal's MST
 *  (uses edge sorting + Disjoint Set Union). The MST gives the cheapest
 *  set of roads that still connects every hub (a minimum-cost route plan).
 * ------------------------------------------------------------- */
class CityGraph {
public:
    static const int N = 8;
private:
    struct Edge { int u, v, w; };
    string name[N];
    double hx[N], hy[N];
    int matrix[N][N];                 // 0 = no direct road, else distance in km
    vector<pair<int, int> > adj[N];   // (neighbour, distance)
    vector<Edge> edges;

    void addRoad(int u, int v, int w) {
        matrix[u][v] = matrix[v][u] = w;
        adj[u].push_back(make_pair(v, w));
        adj[v].push_back(make_pair(u, w));
        Edge e = {u, v, w};
        edges.push_back(e);
    }
    void dfsVisit(int u, vector<bool> &seen, vector<int> &order) const {
        seen[u] = true;
        order.push_back(u);
        for (size_t i = 0; i < adj[u].size(); i++)
            if (!seen[adj[u][i].first]) dfsVisit(adj[u][i].first, seen, order);
    }
    static int findSet(vector<int> &p, int x) {     // DSU with path compression
        return p[x] == x ? x : (p[x] = findSet(p, p[x]));
    }
    void printTree(const vector<Edge> &t, int total) const {
        for (size_t i = 0; i < t.size(); i++)
            cout << "   " << name[t[i].u] << " <-> " << name[t[i].v] << "  (" << t[i].w << " km)\n";
        cout << "   Total road length = " << total << " km (" << t.size() << " roads)\n";
    }
public:
    CityGraph() {
        const char *nm[N] = {"Central Market", "North Station", "East Colony", "South Park",
                             "West Enclave", "NE Sector", "SE Sector", "NW Campus"};
        double xs[N] = {50, 50, 85, 50, 15, 80, 80, 20};
        double ys[N] = {50, 85, 50, 15, 50, 80, 20, 80};
        for (int i = 0; i < N; i++) {
            name[i] = nm[i]; hx[i] = xs[i]; hy[i] = ys[i];
            for (int j = 0; j < N; j++) matrix[i][j] = 0;
        }
        addRoad(0, 1, 35); addRoad(0, 2, 35); addRoad(0, 3, 35); addRoad(0, 4, 35);
        addRoad(1, 5, 31); addRoad(1, 7, 31); addRoad(2, 5, 31); addRoad(2, 6, 31);
        addRoad(3, 6, 31); addRoad(4, 7, 31); addRoad(0, 5, 45); addRoad(0, 6, 45);
        addRoad(3, 4, 50);
    }
    string hubName(int i) const { return name[i]; }

    int nearestHub(double x, double y) const {
        int best = 0;
        double bd = 1e9;
        for (int i = 0; i < N; i++) {
            double d = sqrt((x - hx[i]) * (x - hx[i]) + (y - hy[i]) * (y - hy[i]));
            if (d < bd) { bd = d; best = i; }
        }
        return best;
    }

    void printHubs() const {
        for (int i = 0; i < N; i++)
            cout << "   [" << i << "] " << name[i] << " (" << hx[i] << ", " << hy[i] << ")\n";
    }

    void printMatrix() const {
        cout << "\n  Adjacency Matrix (km, 0 = no direct road)\n      ";
        for (int j = 0; j < N; j++) cout << setw(4) << j;
        cout << "\n";
        for (int i = 0; i < N; i++) {
            cout << "  " << setw(3) << i << " ";
            for (int j = 0; j < N; j++) cout << setw(4) << matrix[i][j];
            cout << "\n";
        }
    }

    void printList() const {
        cout << "\n  Adjacency List\n";
        for (int i = 0; i < N; i++) {
            cout << "  " << i << " (" << name[i] << ") -> ";
            for (size_t j = 0; j < adj[i].size(); j++)
                cout << adj[i][j].first << "[" << adj[i][j].second << "km] ";
            cout << "\n";
        }
    }

    /* BFS: visits hubs level by level; parent[] lets us rebuild shortest-hop paths */
    vector<int> bfs(int src, vector<int> &level) const {
        vector<int> order, parent(N, -1);
        vector<bool> seen(N, false);
        level.assign(N, -1);
        IntakeQueue q;                       // reuse our own linked-list queue
        q.enqueue(src); seen[src] = true; level[src] = 0;
        while (!q.empty()) {
            int u = q.dequeue();
            order.push_back(u);
            for (size_t i = 0; i < adj[u].size(); i++) {
                int v = adj[u][i].first;
                if (!seen[v]) { seen[v] = true; parent[v] = u; level[v] = level[u] + 1; q.enqueue(v); }
            }
        }
        return parent;
    }

    void showBFS(int src) const {
        vector<int> level;
        vector<int> parent = bfs(src, level);
        cout << "\n  BFS from " << name[src] << " (hub, hops from source):\n   ";
        for (int l = 0; l < N; l++)
            for (int v = 0; v < N; v++) if (level[v] == l) cout << name[v] << "(" << l << ")  ";
        cout << "\n";
    }

    void showDFS(int src) const {
        vector<bool> seen(N, false);
        vector<int> order;
        dfsVisit(src, seen, order);
        cout << "\n  DFS from " << name[src] << ":\n   ";
        for (size_t i = 0; i < order.size(); i++) cout << name[order[i]] << (i + 1 < order.size() ? " -> " : "\n");
    }

    string route(int s, int t) const {       // fewest-hops route using BFS parents
        vector<int> level;
        vector<int> parent = bfs(s, level);
        vector<int> path;
        for (int v = t; v != -1; v = parent[v]) path.push_back(v);
        reverse(path.begin(), path.end());
        string out;
        for (size_t i = 0; i < path.size(); i++) out += (i ? " -> " : "") + name[path[i]];
        return out;
    }

    /* Prim's: grow the tree from hub 0, always taking the cheapest edge leaving it */
    void primMST() const {
        vector<bool> inTree(N, false);
        vector<Edge> tree;
        int total = 0;
        MinHeap pq;                          // key = weight, tie = parent, id = vertex
        pq.push(HeapItem{0, 0, 0});
        while (!pq.empty() && (int)tree.size() < N - 1) {
            HeapItem t = pq.pop();
            int v = t.id;
            if (inTree[v]) continue;
            inTree[v] = true;
            if (v != 0) { Edge e = {t.tie, v, t.key}; tree.push_back(e); total += t.key; }
            for (size_t i = 0; i < adj[v].size(); i++)
                if (!inTree[adj[v][i].first]) pq.push(HeapItem{adj[v][i].second, v, adj[v][i].first});
        }
        cout << "\n  Prim's MST (cheapest road network connecting all hubs):\n";
        printTree(tree, total);
    }

    /* Kruskal's: sort all edges, add an edge only if it joins two different components */
    void kruskalMST() const {
        vector<Edge> sorted = edges;
        for (size_t i = 1; i < sorted.size(); i++) {         // insertion sort by weight
            Edge k = sorted[i];
            int j = i - 1;
            while (j >= 0 && sorted[j].w > k.w) { sorted[j + 1] = sorted[j]; j--; }
            sorted[j + 1] = k;
        }
        vector<int> parent(N);
        for (int i = 0; i < N; i++) parent[i] = i;
        vector<Edge> tree;
        int total = 0;
        for (size_t i = 0; i < sorted.size() && (int)tree.size() < N - 1; i++) {
            int a = findSet(parent, sorted[i].u), b = findSet(parent, sorted[i].v);
            if (a != b) { parent[a] = b; tree.push_back(sorted[i]); total += sorted[i].w; }
        }
        cout << "\n  Kruskal's MST (same goal, edge-sorting approach):\n";
        printTree(tree, total);
    }
};

/* ---------------------------------------------------------------
 *  The main system that ties everything together
 * ------------------------------------------------------------- */
class FoodRescueSystem {
    ProfileMap profiles;
    vector<Donation> donations;     // donation id = index + 1
    vector<Request> requests;       // request id  = index + 1
    MinHeap donationHeap, requestHeap;
    IntakeQueue donationQueue, requestQueue;
    GridIndex grid;
    CityGraph city;
    MatchHistory history;
    int clockHour, nextMatchId, donorCount, recipCount;

    string newId(char type) {
        int n = (type == 'D') ? ++donorCount : ++recipCount;
        char buf[16];
        sprintf(buf, "%c%03d", type, n);
        return string(buf);
    }
    static double dist(double x1, double y1, double x2, double y2) {
        return sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
    }

    /* ---- intake: queue -> heap + grid ---- */
    void processIntake() {
        while (!donationQueue.empty()) {
            Donation &d = donations[donationQueue.dequeue() - 1];
            d.status = "OPEN";
            donationHeap.push({d.expiryTime, d.id, d.id});
            grid.insert(d.id, d.x, d.y);
        }
        while (!requestQueue.empty()) {
            Request &r = requests[requestQueue.dequeue() - 1];
            r.status = "OPEN";
            requestHeap.push({r.urgency, r.id, r.id});
        }
    }

    /* remove spoiled food from the top of the donation heap */
    void purgeExpired() {
        while (!donationHeap.empty()) {
            HeapItem top = donationHeap.top();            // peek
            Donation &d = donations[top.id - 1];
            if (d.status != "OPEN") { donationHeap.pop(); continue; }   // lazy delete
            if (d.expiryTime > clockHour) break;          // soonest-expiring is still fine
            donationHeap.pop();
            d.status = "EXPIRED";
            grid.remove(d.id, d.x, d.y);
            cout << "  [Expired] Donation #" << d.id << " (" << d.food << ") spoiled and was removed.\n";
        }
    }

    /* best donation for a request: nearest first, then earliest expiry */
    int findBestDonation(const Request &r, bool &blockedByProfile) {
        vector<int> cand = grid.nearby(r.x, r.y, MAX_RADIUS);
        int best = -1;
        double bestDist = 1e9;
        for (size_t i = 0; i < cand.size(); i++) {
            Donation &d = donations[cand[i] - 1];
            if (d.status != "OPEN" || d.remaining <= 0) continue;
            if (toLower(d.food) != toLower(r.food)) continue;
            if (d.expiryTime <= clockHour) continue;
            double dd = dist(r.x, r.y, d.x, d.y);
            if (dd > MAX_RADIUS) continue;
            Profile *donor = profiles.get(d.donorId);       // hash lookup
            if (!donor || !donor->complete()) { blockedByProfile = true; continue; }
            if (best == -1 || dd < bestDist ||
                (dd == bestDist && d.expiryTime < donations[best - 1].expiryTime)) {
                best = d.id;
                bestDist = dd;
            }
        }
        return best;
    }

    /* ---- main matching loop ---- */
    void runMatching() {
        processIntake();
        purgeExpired();
        vector<HeapItem> leftover;     // requests we could not serve this round
        while (!requestHeap.empty()) {
            HeapItem top = requestHeap.pop();               // most urgent request
            Request &r = requests[top.id - 1];
            if (r.status != "OPEN") continue;
            Profile *rp = profiles.get(r.recipId);
            if (!rp || !rp->complete()) {
                cout << "  [Profile Error] Recipient " << r.recipId
                     << " has an incomplete/inactive profile. Request #" << r.id << " stays queued.\n";
                leftover.push_back(top);
                continue;
            }
            while (r.remaining > 0) {
                bool blocked = false;
                int did = findBestDonation(r, blocked);
                if (did == -1) {
                    if (blocked)
                        cout << "  [Profile Error] A nearby donor's profile is incomplete. No match for request #"
                             << r.id << ".\n";
                    else
                        cout << "  [Insufficient Match] Request #" << r.id << " (" << r.food
                             << ") stays queued.\n";
                    break;
                }
                createMatch(donations[did - 1], r);
            }
            if (r.remaining > 0) leftover.push_back(top);
            else r.status = "FULFILLED";
        }
        for (size_t i = 0; i < leftover.size(); i++) requestHeap.push(leftover[i]);
    }

    void createMatch(Donation &d, Request &r) {
        int q = min(d.remaining, r.remaining);
        d.remaining -= q;
        r.remaining -= q;
        if (d.remaining == 0) { d.status = "MATCHED"; grid.remove(d.id, d.x, d.y); }
        Match m;
        m.id = ++nextMatchId; m.donationId = d.id; m.requestId = r.id;
        m.donorId = d.donorId; m.recipId = r.recipId; m.food = d.food;
        m.qty = q; m.dist = dist(d.x, d.y, r.x, r.y); m.time = clockHour;
        history.add(m);
        cout << "\n  *** MATCH #" << m.id << " CREATED ***\n"
             << "  Notify donor     " << m.donorId << ": " << q << " x " << m.food << " will be picked up by "
             << m.recipId << " (" << fixed << setprecision(1) << m.dist << " km away).\n"
             << "  Notify recipient " << m.recipId << ": " << q << " x " << m.food << " available from "
             << m.donorId << ".\n"
             << "  Delivery route (BFS, fewest road hops): "
             << city.route(city.nearestHub(d.x, d.y), city.nearestHub(r.x, r.y)) << "\n\n";
    }

public:
    FoodRescueSystem() : clockHour(0), nextMatchId(0), donorCount(0), recipCount(0) {}

    /* ---------- profile management ---------- */
    string registerProfile() {
        Profile p;
        cout << "\n-- Register --\n";
        int t = readInt("Register as 1) Donor  2) Recipient : ", 1, 2);
        p.type = (t == 1) ? 'D' : 'R';
        p.name = readLine("Name: ");
        p.phone = readLine("Phone (type - to skip for now): ");
        cout << "Location on the city grid (0-100 km).\n";
        p.x = readInt("  X: ", 0, 100);
        p.y = readInt("  Y: ", 0, 100);
        p.active = true;
        p.id = newId(p.type);
        profiles.put(p);
        cout << "Registered! Your ID is " << p.id << (p.complete() ? "" : "  (profile INCOMPLETE - add phone in Manage Profile)") << "\n";
        return p.id;
    }

    string login() {
        string id = readLine("Enter your profile ID: ");
        for (size_t i = 0; i < id.size(); i++) id[i] = toupper(id[i]);
        if (!profiles.get(id)) { cout << "No such profile.\n"; return ""; }
        return id;
    }

    void showProfile(const Profile &p) {
        cout << "\n  ID: " << p.id << "   Type: " << (p.type == 'D' ? "Donor" : "Recipient")
             << "\n  Name: " << p.name << "   Phone: " << p.phone
             << "\n  Location: (" << p.x << ", " << p.y << ")"
             << "   Nearest hub: " << city.hubName(city.nearestHub(p.x, p.y))
             << "\n  Status: " << (p.active ? "Active" : "Inactive")
             << ", " << (p.complete() ? "Complete" : "INCOMPLETE") << "\n";
    }

    void manageProfile(const string &id) {
        Profile *p = profiles.get(id);
        while (true) {
            cout << "\n--- Manage Profile ---\n1. View profile\n2. Update phone\n3. Update location\n"
                 << "4. Activate / deactivate\n5. My donations / requests\n6. My match history\n7. Back\n";
            int c = readInt("Choice: ", 1, 7);
            if (c == 1) showProfile(*p);
            else if (c == 2) p->phone = readLine("New phone: ");
            else if (c == 3) { p->x = readInt("  X: ", 0, 100); p->y = readInt("  Y: ", 0, 100); }
            else if (c == 4) { p->active = !p->active; cout << "Profile is now " << (p->active ? "active" : "inactive") << ".\n"; }
            else if (c == 5) listMine(*p);
            else if (c == 6) { if (history.print(id) == 0) cout << "  No matches yet.\n"; }
            else return;
        }
    }

    void listMine(const Profile &p) {
        int shown = 0;
        if (p.type == 'D') {
            for (size_t i = 0; i < donations.size(); i++)
                if (donations[i].donorId == p.id) {
                    const Donation &d = donations[i];
                    cout << "  Donation #" << d.id << " " << d.food << " left " << d.remaining << "/" << d.qty
                         << " expires hour " << d.expiryTime << " [" << d.status << "]\n";
                    shown++;
                }
        } else {
            for (size_t i = 0; i < requests.size(); i++)
                if (requests[i].recipId == p.id) {
                    const Request &r = requests[i];
                    cout << "  Request #" << r.id << " " << r.food << " needs " << r.remaining << "/" << r.qty
                         << " urgency " << r.urgency << " [" << r.status << "]\n";
                    shown++;
                }
        }
        if (!shown) cout << "  Nothing yet.\n";
    }

    /* ---------- donate / request ---------- */
    void donateFood(const string &id) {
        Profile *p = profiles.get(id);
        if (p->type != 'D') { cout << "Only donors can donate food.\n"; return; }
        if (!p->complete()) { cout << "[Profile Error] Complete your profile first.\n"; return; }
        cout << "\n-- Donate Food --\n";
        Donation d;
        d.food = readLine("Food type (one word, e.g. rice, bread): ");
        d.qty = readInt("Quantity (meals/units): ", 1, 10000);
        int hrs = readInt("Hours until it expires: ", 0, 500);
        int quality = readInt("Quality check - is the food fresh and sealed/covered? (1=yes, 0=no): ", 0, 1);
        if (hrs < MIN_SHELF_HOURS || !quality || d.food.empty()) {    // food-safety validation
            cout << "[Donation Error] Food failed the safety check (expiry/quality). Not accepted.\n";
            return;
        }
        d.id = donations.size() + 1;
        d.donorId = id; d.remaining = d.qty; d.expiryTime = clockHour + hrs;
        d.x = p->x; d.y = p->y; d.status = "QUEUED";
        donations.push_back(d);
        donationQueue.enqueue(d.id);
        cout << "Donation #" << d.id << " accepted and queued.\n";
        runMatching();
    }

    void requestFood(const string &id) {
        Profile *p = profiles.get(id);
        if (p->type != 'R') { cout << "Only recipients can request food.\n"; return; }
        if (!p->complete()) { cout << "[Profile Error] Complete your profile first.\n"; return; }
        cout << "\n-- Request Food --\n";
        Request r;
        r.food = readLine("Food type needed: ");
        r.qty = readInt("Quantity needed: ", 1, 10000);
        r.urgency = readInt("Urgency (1 = critical ... 5 = low): ", 1, 5);
        r.id = requests.size() + 1;
        r.recipId = id; r.remaining = r.qty; r.x = p->x; r.y = p->y; r.status = "QUEUED";
        requests.push_back(r);
        requestQueue.enqueue(r.id);
        cout << "Request #" << r.id << " queued.\n";
        runMatching();
    }

    /* ---------- misc ---------- */
    void advanceClock() {
        int h = readInt("Advance clock by how many hours? ", 1, 100);
        clockHour += h;
        cout << "Clock is now hour " << clockHour << ".\n";
        purgeExpired();
        runMatching();
    }

    void showStatus() {
        int openD = 0, openR = 0;
        for (size_t i = 0; i < donations.size(); i++) if (donations[i].status == "OPEN") openD++;
        for (size_t i = 0; i < requests.size(); i++) if (requests[i].status == "OPEN") openR++;
        cout << "\n=== System Status (hour " << clockHour << ") ===\n"
             << "Profiles: " << profiles.size() << "   Open donations: " << openD
             << "   Open requests: " << openR << "   Matches: " << history.size() << "\n";
        if (!donationHeap.empty()) {
            HeapItem t = donationHeap.top();
            cout << "Next food to expire: donation #" << t.id << " at hour " << t.key << "\n";
        }
        if (!requestHeap.empty()) {
            HeapItem t = requestHeap.top();
            cout << "Most urgent request: #" << t.id << " (urgency " << t.key << ")\n";
        }
        cout << "All matches:\n";
        if (history.print("") == 0) cout << "  None.\n";
    }

    /* ---------- file persistence ---------- */
    void save() {
        ofstream f("profiles.txt");
        vector<Profile> all;
        profiles.collect(all);
        for (size_t i = 0; i < all.size(); i++)
            f << all[i].id << '|' << all[i].name << '|' << all[i].type << '|' << all[i].phone << '|'
              << all[i].x << '|' << all[i].y << '|' << all[i].active << '\n';
        f.close();
        ofstream fd("donations.txt");
        for (size_t i = 0; i < donations.size(); i++) {
            const Donation &d = donations[i];
            fd << d.id << '|' << d.donorId << '|' << d.food << '|' << d.qty << '|' << d.remaining << '|'
               << d.expiryTime << '|' << d.x << '|' << d.y << '|' << d.status << '\n';
        }
        fd.close();
        ofstream fr("requests.txt");
        for (size_t i = 0; i < requests.size(); i++) {
            const Request &r = requests[i];
            fr << r.id << '|' << r.recipId << '|' << r.food << '|' << r.qty << '|' << r.remaining << '|'
               << r.urgency << '|' << r.x << '|' << r.y << '|' << r.status << '\n';
        }
        fr.close();
        ofstream fm("matches.txt");
        history.save(fm);
        fm.close();
        ofstream fx("meta.txt");
        fx << clockHour << ' ' << nextMatchId << ' ' << donorCount << ' ' << recipCount << '\n';
        cout << "Data saved to files.\n";
    }

    void load() {
        string line;
        ifstream fx("meta.txt");
        if (!fx) { cout << "(No saved data found, starting fresh.)\n"; return; }
        fx >> clockHour >> nextMatchId >> donorCount >> recipCount;

        ifstream f("profiles.txt");
        while (getline(f, line)) {
            vector<string> v = split(line, '|');
            if (v.size() < 7) continue;
            Profile p;
            p.id = v[0]; p.name = v[1]; p.type = v[2][0]; p.phone = v[3];
            p.x = atof(v[4].c_str()); p.y = atof(v[5].c_str()); p.active = (v[6] == "1");
            profiles.put(p);
        }
        ifstream fd("donations.txt");
        while (getline(fd, line)) {
            vector<string> v = split(line, '|');
            if (v.size() < 9) continue;
            Donation d;
            d.id = atoi(v[0].c_str()); d.donorId = v[1]; d.food = v[2];
            d.qty = atoi(v[3].c_str()); d.remaining = atoi(v[4].c_str()); d.expiryTime = atoi(v[5].c_str());
            d.x = atof(v[6].c_str()); d.y = atof(v[7].c_str()); d.status = v[8];
            donations.push_back(d);
            if (d.status == "OPEN" || d.status == "QUEUED") {    // rebuild heap + grid
                donationHeap.push({d.expiryTime, d.id, d.id});
                grid.insert(d.id, d.x, d.y);
                donations.back().status = "OPEN";
            }
        }
        ifstream fr("requests.txt");
        while (getline(fr, line)) {
            vector<string> v = split(line, '|');
            if (v.size() < 9) continue;
            Request r;
            r.id = atoi(v[0].c_str()); r.recipId = v[1]; r.food = v[2];
            r.qty = atoi(v[3].c_str()); r.remaining = atoi(v[4].c_str()); r.urgency = atoi(v[5].c_str());
            r.x = atof(v[6].c_str()); r.y = atof(v[7].c_str()); r.status = v[8];
            requests.push_back(r);
            if (r.status == "OPEN" || r.status == "QUEUED") {
                requestHeap.push({r.urgency, r.id, r.id});
                requests.back().status = "OPEN";
            }
        }
        ifstream fm("matches.txt");
        while (getline(fm, line)) {
            vector<string> v = split(line, '|');
            if (v.size() < 9) continue;
            Match m;
            m.id = atoi(v[0].c_str()); m.donationId = atoi(v[1].c_str()); m.requestId = atoi(v[2].c_str());
            m.donorId = v[3]; m.recipId = v[4]; m.food = v[5]; m.qty = atoi(v[6].c_str());
            m.dist = atof(v[7].c_str()); m.time = atoi(v[8].c_str());
            history.add(m);
        }
        cout << "Loaded " << profiles.size() << " profiles, " << donations.size() << " donations, "
             << requests.size() << " requests, " << history.size() << " matches.\n";
    }

    /* ---------- menus ---------- */
    void userMenu(const string &id) {
        while (true) {
            Profile *p = profiles.get(id);
            cout << "\n===== Welcome " << p->name << " (" << (p->type == 'D' ? "Donor" : "Recipient") << ") =====\n"
                 << "1. Create & Manage Profile\n2. Donate Food\n3. Request Food\n4. Logout\n";
            int c = readInt("Choice: ", 1, 4);
            if (c == 1) manageProfile(id);
            else if (c == 2) donateFood(id);
            else if (c == 3) requestFood(id);
            else { cout << "Logged out.\n"; return; }
        }
    }

    void graphMenu() {
        while (true) {
            cout << "\n--- City Road Network (Graph) ---\n"
                 << "1. Show hubs\n2. Adjacency matrix\n3. Adjacency list\n4. BFS traversal\n"
                 << "5. DFS traversal\n6. Prim's MST\n7. Kruskal's MST\n8. Back\n";
            int c = readInt("Choice: ", 1, 8);
            if (c == 1) city.printHubs();
            else if (c == 2) city.printMatrix();
            else if (c == 3) city.printList();
            else if (c == 4 || c == 5) {
                city.printHubs();
                int src = readInt("Start hub number: ", 0, CityGraph::N - 1);
                if (c == 4) city.showBFS(src); else city.showDFS(src);
            }
            else if (c == 6) city.primMST();
            else if (c == 7) city.kruskalMST();
            else return;
        }
    }

    void run() {
        load();
        while (true) {
            cout << "\n########## FOOD RESCUE & DONATION MATCHER ##########\n"
                 << "1. Register new profile\n2. Login\n3. System status & all matches\n"
                 << "4. Advance clock (simulate time passing)\n5. City road network (graph)\n6. Save & Exit\n";
            int c = readInt("Choice: ", 1, 6);
            if (c == 1) { string id = registerProfile(); userMenu(id); }
            else if (c == 2) { string id = login(); if (!id.empty()) userMenu(id); }
            else if (c == 3) showStatus();
            else if (c == 4) advanceClock();
            else if (c == 5) graphMenu();
            else { save(); cout << "Goodbye! Thank you for fighting food waste.\n"; return; }
        }
    }
};

int main() {
    FoodRescueSystem app;
    app.run();
    return 0;
}
