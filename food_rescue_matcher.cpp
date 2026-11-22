#include <iostream>
#include <fstream>
#include <string>
#include <cmath>
using namespace std;

const int MAXN = 500, GRID = 10, CELL = 10, CAP = 50, NH = 8;
const double RADIUS = 15;
enum { QUEUED, OPEN, DONE, EXPIRED };

struct Profile {
    string id, name, phone;
    char type;
    double x, y;
    bool active;
    bool complete() { return active && phone != "-" && name != ""; }
};
struct Donation {
    int id, qty, left, expiry, status;
    string donor, food;
    double x, y;
};
struct Request {
    int id, qty, left, urgency, status;
    string recip, food;
    double x, y;
};
struct Match {
    int id, qty, time;
    string donor, recip, food;
    double dist;
};

double dist(double x1, double y1, double x2, double y2) {
    return sqrt((x1 - x2) * (x1 - x2) + (y1 - y2) * (y1 - y2));
}

int readInt(string prompt, int lo, int hi) {
    int v;
    while (true) {
        cout << prompt;
        if (cin >> v && v >= lo && v <= hi) { cin.ignore(1000, '\n'); return v; }
        cin.clear();
        cin.ignore(1000, '\n');
        cout << "  Enter a number between " << lo << " and " << hi << ".\n";
    }
}

string readWord(string prompt) {
    string s;
    cout << prompt;
    cin >> s;
    cin.ignore(1000, '\n');
    return s;
}

struct PNode { Profile p; PNode *next; };

class ProfileMap {
    PNode *table[53];
    int count;
    int hash(string key) {
        int h = 0;
        for (int i = 0; i < (int)key.size(); i++) h = (h * 31 + key[i]) % 53;
        return h;
    }
public:
    ProfileMap() { count = 0; for (int i = 0; i < 53; i++) table[i] = NULL; }
    void put(Profile p) {
        int h = hash(p.id);
        table[h] = new PNode{p, table[h]};
        count++;
    }
    Profile *get(string id) {
        for (PNode *n = table[hash(id)]; n; n = n->next)
            if (n->p.id == id) return &n->p;
        return NULL;
    }
    int size() { return count; }
    void save(ofstream &f) {
        for (int i = 0; i < 53; i++)
            for (PNode *n = table[i]; n; n = n->next)
                f << n->p.id << ' ' << n->p.type << ' ' << n->p.phone << ' ' << n->p.x << ' '
                  << n->p.y << ' ' << n->p.active << ' ' << n->p.name << '\n';
    }
};

struct Item { int key, tie, id; };

bool before(Item a, Item b) { return a.key < b.key || (a.key == b.key && a.tie < b.tie); }

class MinHeap {
    Item a[MAXN + 50];
    int n;
public:
    MinHeap() { n = 0; }
    bool empty() { return n == 0; }
    Item top() { return a[0]; }
    void push(Item x) {
        int i = n++;
        a[i] = x;
        while (i > 0 && before(a[i], a[(i - 1) / 2])) {
            swap(a[i], a[(i - 1) / 2]);
            i = (i - 1) / 2;
        }
    }
    Item pop() {
        Item t = a[0];
        a[0] = a[--n];
        int i = 0;
        while (true) {
            int l = 2 * i + 1, r = l + 1, m = i;
            if (l < n && before(a[l], a[m])) m = l;
            if (r < n && before(a[r], a[m])) m = r;
            if (m == i) break;
            swap(a[i], a[m]);
            i = m;
        }
        return t;
    }
};

struct QNode { int v; QNode *next; };

class Queue {
    QNode *head, *tail;
public:
    Queue() { head = tail = NULL; }
    bool empty() { return head == NULL; }
    void push(int v) {
        QNode *nn = new QNode{v, NULL};
        if (tail) tail->next = nn; else head = nn;
        tail = nn;
    }
    int pop() {
        QNode *t = head;
        int v = t->v;
        head = head->next;
        if (!head) tail = NULL;
        delete t;
        return v;
    }
};

class Grid {
    int cell[GRID][GRID][CAP], cnt[GRID][GRID];
    int c(double v) {
        int i = (int)(v / CELL);
        return i < 0 ? 0 : (i >= GRID ? GRID - 1 : i);
    }
public:
    Grid() {
        for (int i = 0; i < GRID; i++)
            for (int j = 0; j < GRID; j++) cnt[i][j] = 0;
    }
    void add(int id, double x, double y) {
        int i = c(x), j = c(y);
        if (cnt[i][j] < CAP) cell[i][j][cnt[i][j]++] = id;
    }
    void del(int id, double x, double y) {
        int i = c(x), j = c(y);
        for (int k = 0; k < cnt[i][j]; k++)
            if (cell[i][j][k] == id) {
                cell[i][j][k] = cell[i][j][--cnt[i][j]];
                return;
            }
    }
    int around(double x, double y, int out[]) {
        int n = 0;
        for (int i = c(x - RADIUS); i <= c(x + RADIUS); i++)
            for (int j = c(y - RADIUS); j <= c(y + RADIUS); j++)
                for (int k = 0; k < cnt[i][j]; k++) out[n++] = cell[i][j][k];
        return n;
    }
};

struct MNode { Match m; MNode *next; };

class History {
    MNode *head, *tail;
    int n;
public:
    History() { head = tail = NULL; n = 0; }
    int size() { return n; }
    void add(Match m) {
        MNode *nn = new MNode{m, NULL};
        if (tail) tail->next = nn; else head = nn;
        tail = nn;
        n++;
    }
    int print(string owner) {
        int shown = 0;
        for (MNode *p = head; p; p = p->next) {
            if (owner != "" && p->m.donor != owner && p->m.recip != owner) continue;
            cout << "  Match #" << p->m.id << " | " << p->m.qty << " x " << p->m.food << " | "
                 << p->m.donor << " -> " << p->m.recip << " | " << (int)(p->m.dist * 10) / 10.0
                 << " km | hour " << p->m.time << "\n";
            shown++;
        }
        return shown;
    }
    void save(ofstream &f) {
        for (MNode *p = head; p; p = p->next)
            f << p->m.id << ' ' << p->m.donor << ' ' << p->m.recip << ' ' << p->m.food << ' '
              << p->m.qty << ' ' << p->m.dist << ' ' << p->m.time << '\n';
    }
};

class Graph {
    string name[NH];
    double hx[NH], hy[NH];
    int mat[NH][NH];
    int adjV[NH][NH], adjW[NH][NH], adjN[NH];
    int eu[20], ev[20], ew[20], ne;

    void road(int u, int v, int w) {
        mat[u][v] = mat[v][u] = w;
        adjV[u][adjN[u]] = v; adjW[u][adjN[u]++] = w;
        adjV[v][adjN[v]] = u; adjW[v][adjN[v]++] = w;
        eu[ne] = u; ev[ne] = v; ew[ne++] = w;
    }
    void bfs(int s, int par[], int lev[], int order[]) {
        Queue q;
        int cnt = 0;
        for (int i = 0; i < NH; i++) { par[i] = -1; lev[i] = -1; }
        q.push(s);
        lev[s] = 0;
        while (!q.empty()) {
            int u = q.pop();
            order[cnt++] = u;
            for (int k = 0; k < adjN[u]; k++) {
                int v = adjV[u][k];
                if (lev[v] == -1) { lev[v] = lev[u] + 1; par[v] = u; q.push(v); }
            }
        }
    }
    void dfs(int u, bool seen[]) {
        seen[u] = true;
        cout << name[u] << "  ";
        for (int k = 0; k < adjN[u]; k++)
            if (!seen[adjV[u][k]]) dfs(adjV[u][k], seen);
    }
    int find(int p[], int x) {
        while (p[x] != x) x = p[x];
        return x;
    }
public:
    Graph() {
        string nm[NH] = {"Central Market", "North Station", "East Colony", "South Park",
                         "West Enclave", "NE Sector", "SE Sector", "NW Campus"};
        double xs[NH] = {50, 50, 85, 50, 15, 80, 80, 20};
        double ys[NH] = {50, 85, 50, 15, 50, 80, 20, 80};
        ne = 0;
        for (int i = 0; i < NH; i++) {
            name[i] = nm[i]; hx[i] = xs[i]; hy[i] = ys[i]; adjN[i] = 0;
            for (int j = 0; j < NH; j++) mat[i][j] = 0;
        }
        road(0, 1, 35); road(0, 2, 35); road(0, 3, 35); road(0, 4, 35);
        road(1, 5, 31); road(1, 7, 31); road(2, 5, 31); road(2, 6, 31);
        road(3, 6, 31); road(4, 7, 31); road(0, 5, 45); road(0, 6, 45);
        road(3, 4, 50);
    }
    string hubName(int i) { return name[i]; }
    int nearest(double x, double y) {
        int best = 0;
        for (int i = 1; i < NH; i++)
            if (dist(x, y, hx[i], hy[i]) < dist(x, y, hx[best], hy[best])) best = i;
        return best;
    }
    void showHubs() {
        for (int i = 0; i < NH; i++) cout << "  [" << i << "] " << name[i] << "\n";
    }
    void showMatrix() {
        cout << "\n  Adjacency matrix (km, 0 = no road)\n";
        for (int i = 0; i < NH; i++) {
            cout << "  ";
            for (int j = 0; j < NH; j++) cout << mat[i][j] << "\t";
            cout << "\n";
        }
    }
    void showList() {
        cout << "\n  Adjacency list\n";
        for (int i = 0; i < NH; i++) {
            cout << "  " << i << " " << name[i] << " -> ";
            for (int k = 0; k < adjN[i]; k++) cout << adjV[i][k] << "(" << adjW[i][k] << "km) ";
            cout << "\n";
        }
    }
    void showBFS(int s) {
        int par[NH], lev[NH], order[NH];
        bfs(s, par, lev, order);
        cout << "\n  BFS from " << name[s] << ":\n  ";
        for (int i = 0; i < NH; i++) cout << name[order[i]] << "(" << lev[order[i]] << ")  ";
        cout << "\n";
    }
    void showDFS(int s) {
        bool seen[NH] = {false};
        cout << "\n  DFS from " << name[s] << ":\n  ";
        dfs(s, seen);
        cout << "\n";
    }
    string route(int s, int t) {
        int par[NH], lev[NH], order[NH], path[NH], len = 0;
        bfs(s, par, lev, order);
        for (int v = t; v != -1; v = par[v]) path[len++] = v;
        string out = "";
        for (int i = len - 1; i >= 0; i--) out += name[path[i]] + (i > 0 ? " -> " : "");
        return out;
    }
    void prim() {
        bool in[NH] = {false};
        MinHeap pq;
        int total = 0;
        cout << "\n  Prim's MST:\n";
        pq.push({0, 0, 0});
        while (!pq.empty()) {
            Item t = pq.pop();
            if (in[t.id]) continue;
            in[t.id] = true;
            if (t.id != 0) {
                cout << "   " << name[t.tie] << " - " << name[t.id] << " (" << t.key << " km)\n";
                total += t.key;
            }
            for (int k = 0; k < adjN[t.id]; k++)
                if (!in[adjV[t.id][k]]) pq.push({adjW[t.id][k], t.id, adjV[t.id][k]});
        }
        cout << "   Total = " << total << " km\n";
    }
    void kruskal() {
        int idx[20], p[NH], total = 0;
        for (int i = 0; i < ne; i++) idx[i] = i;
        for (int i = 1; i < ne; i++) {
            int k = idx[i], j = i - 1;
            while (j >= 0 && ew[idx[j]] > ew[k]) { idx[j + 1] = idx[j]; j--; }
            idx[j + 1] = k;
        }
        for (int i = 0; i < NH; i++) p[i] = i;
        cout << "\n  Kruskal's MST:\n";
        for (int i = 0; i < ne; i++) {
            int e = idx[i], a = find(p, eu[e]), b = find(p, ev[e]);
            if (a == b) continue;
            p[a] = b;
            cout << "   " << name[eu[e]] << " - " << name[ev[e]] << " (" << ew[e] << " km)\n";
            total += ew[e];
        }
        cout << "   Total = " << total << " km\n";
    }
};

class System {
    ProfileMap profiles;
    Donation don[MAXN];
    Request req[MAXN];
    int nd, nr, clockHour, nextMatch, countD, countR;
    MinHeap donHeap, reqHeap;
    Queue donQueue, reqQueue;
    Grid grid;
    Graph city;
    History history;

    void intake() {
        while (!donQueue.empty()) {
            Donation &d = don[donQueue.pop() - 1];
            d.status = OPEN;
            donHeap.push({d.expiry, d.id, d.id});
            grid.add(d.id, d.x, d.y);
        }
        while (!reqQueue.empty()) {
            Request &r = req[reqQueue.pop() - 1];
            r.status = OPEN;
            reqHeap.push({r.urgency, r.id, r.id});
        }
    }

    void expire() {
        while (!donHeap.empty()) {
            Donation &d = don[donHeap.top().id - 1];
            if (d.status == OPEN && d.expiry > clockHour) break;
            donHeap.pop();
            if (d.status != OPEN) continue;
            d.status = EXPIRED;
            grid.del(d.id, d.x, d.y);
            cout << "  [Expired] Donation #" << d.id << " (" << d.food << ") has spoiled.\n";
        }
    }

    int bestDonation(Request &r) {
        int cand[16 * CAP], best = -1;
        int n = grid.around(r.x, r.y, cand);
        for (int i = 0; i < n; i++) {
            Donation &d = don[cand[i] - 1];
            if (d.status != OPEN || d.food != r.food) continue;
            double dd = dist(r.x, r.y, d.x, d.y);
            if (dd > RADIUS) continue;
            Profile *p = profiles.get(d.donor);
            if (!p->complete()) {
                cout << "  [Profile Error] Donor " << d.donor << " has an incomplete profile.\n";
                continue;
            }
            if (best == -1 || dd < dist(r.x, r.y, don[best - 1].x, don[best - 1].y)) best = d.id;
        }
        return best;
    }

    void join(Donation &d, Request &r) {
        Match m;
        m.qty = d.left < r.left ? d.left : r.left;
        d.left -= m.qty;
        r.left -= m.qty;
        if (d.left == 0) { d.status = DONE; grid.del(d.id, d.x, d.y); }
        m.id = ++nextMatch;
        m.donor = d.donor; m.recip = r.recip; m.food = d.food;
        m.dist = dist(d.x, d.y, r.x, r.y);
        m.time = clockHour;
        history.add(m);
        cout << "\n  *** MATCH #" << m.id << " ***\n"
             << "  Notify donor " << m.donor << ": " << m.qty << " x " << m.food
             << " will be collected by " << m.recip << " (" << (int)(m.dist * 10) / 10.0 << " km).\n"
             << "  Notify recipient " << m.recip << ": " << m.qty << " x " << m.food
             << " is ready from " << m.donor << ".\n"
             << "  Route (BFS): " << city.route(city.nearest(d.x, d.y), city.nearest(r.x, r.y)) << "\n\n";
    }

    void matchAll() {
        Item keep[MAXN];
        int k = 0;
        intake();
        expire();
        while (!reqHeap.empty()) {
            Item t = reqHeap.pop();
            Request &r = req[t.id - 1];
            if (r.status != OPEN) continue;
            while (r.left > 0) {
                int d = bestDonation(r);
                if (d == -1) {
                    cout << "  [Insufficient Match] Request #" << r.id << " (" << r.food << ") stays queued.\n";
                    break;
                }
                join(don[d - 1], r);
            }
            if (r.left > 0) keep[k++] = t; else r.status = DONE;
        }
        for (int i = 0; i < k; i++) reqHeap.push(keep[i]);
    }

    void listMine(Profile &p) {
        int shown = 0;
        for (int i = 0; i < nd && p.type == 'D'; i++)
            if (don[i].donor == p.id) {
                cout << "  Donation #" << don[i].id << " " << don[i].food << " left " << don[i].left
                     << "/" << don[i].qty << " (status " << don[i].status << ")\n";
                shown++;
            }
        for (int i = 0; i < nr && p.type == 'R'; i++)
            if (req[i].recip == p.id) {
                cout << "  Request #" << req[i].id << " " << req[i].food << " needs " << req[i].left
                     << "/" << req[i].qty << " (status " << req[i].status << ")\n";
                shown++;
            }
        if (!shown) cout << "  Nothing yet.\n";
    }

    void manageProfile(Profile *p) {
        while (true) {
            cout << "\n--- Manage Profile ---\n1. View profile\n2. Update phone\n3. My donations/requests\n"
                 << "4. My matches\n5. Back\n";
            int c = readInt("Choice: ", 1, 5);
            if (c == 1)
                cout << "  " << p->id << " | " << p->name << " | phone " << p->phone << " | location ("
                     << p->x << "," << p->y << ") | hub " << city.hubName(city.nearest(p->x, p->y))
                     << (p->complete() ? " | complete\n" : " | INCOMPLETE\n");
            else if (c == 2) p->phone = readWord("New phone (one word, - to clear): ");
            else if (c == 3) listMine(*p);
            else if (c == 4) { if (!history.print(p->id)) cout << "  No matches yet.\n"; }
            else return;
        }
    }

    void donate(Profile *p) {
        if (!p->complete()) { cout << "[Profile Error] Complete your profile first.\n"; return; }
        if (nd >= MAXN) { cout << "System full.\n"; return; }
        Donation d;
        d.food = readWord("Food type (one lowercase word): ");
        d.qty = readInt("Quantity: ", 1, 10000);
        int hours = readInt("Hours until expiry: ", 0, 500);
        int fresh = readInt("Is the food fresh and covered? (1=yes, 0=no): ", 0, 1);
        if (hours < 1 || !fresh) {
            cout << "[Donation Error] Food failed the safety check. Not accepted.\n";
            return;
        }
        d.id = ++nd;
        d.donor = p->id; d.left = d.qty; d.expiry = clockHour + hours;
        d.x = p->x; d.y = p->y; d.status = QUEUED;
        don[nd - 1] = d;
        donQueue.push(d.id);
        cout << "Donation #" << d.id << " accepted.\n";
        matchAll();
    }

    void request(Profile *p) {
        if (!p->complete()) { cout << "[Profile Error] Complete your profile first.\n"; return; }
        if (nr >= MAXN) { cout << "System full.\n"; return; }
        Request r;
        r.food = readWord("Food type needed (one lowercase word): ");
        r.qty = readInt("Quantity: ", 1, 10000);
        r.urgency = readInt("Urgency (1 = critical, 5 = low): ", 1, 5);
        r.id = ++nr;
        r.recip = p->id; r.left = r.qty; r.x = p->x; r.y = p->y; r.status = QUEUED;
        req[nr - 1] = r;
        reqQueue.push(r.id);
        cout << "Request #" << r.id << " queued.\n";
        matchAll();
    }

    void userMenu(Profile *p) {
        while (true) {
            cout << "\n===== " << p->name << (p->type == 'D' ? " (Donor)" : " (Recipient)")
                 << " =====\n1. Manage Profile\n2. " << (p->type == 'D' ? "Donate Food" : "Request Food")
                 << "\n3. Logout\n";
            int c = readInt("Choice: ", 1, 3);
            if (c == 1) manageProfile(p);
            else if (c == 2) { if (p->type == 'D') donate(p); else request(p); }
            else return;
        }
    }

    void graphMenu() {
        while (true) {
            cout << "\n--- Road Network ---\n1. Matrix\n2. List\n3. BFS\n4. DFS\n5. Prim\n6. Kruskal\n7. Back\n";
            int c = readInt("Choice: ", 1, 7);
            if (c == 3 || c == 4) {
                city.showHubs();
                int s = readInt("Start hub: ", 0, NH - 1);
                if (c == 3) city.showBFS(s); else city.showDFS(s);
            }
            else if (c == 1) city.showMatrix();
            else if (c == 2) city.showList();
            else if (c == 5) city.prim();
            else if (c == 6) city.kruskal();
            else return;
        }
    }

    void registerUser() {
        Profile p;
        int t = readInt("\nRegister as 1) Donor 2) Recipient: ", 1, 2);
        p.type = t == 1 ? 'D' : 'R';
        cout << "Name: ";
        getline(cin, p.name);
        p.phone = readWord("Phone (one word, - to skip): ");
        p.x = readInt("X (0-100): ", 0, 100);
        p.y = readInt("Y (0-100): ", 0, 100);
        p.active = true;
        int n = p.type == 'D' ? ++countD : ++countR;
        p.id = string(1, p.type) + (n < 10 ? "00" : n < 100 ? "0" : "") + to_string(n);
        profiles.put(p);
        cout << "Your ID is " << p.id << (p.complete() ? "\n" : " (profile INCOMPLETE, add a phone)\n");
        userMenu(profiles.get(p.id));
    }

    void login() {
        string id = readWord("Profile ID: ");
        Profile *p = profiles.get(id);
        if (p) userMenu(p); else cout << "No such profile.\n";
    }

    void status() {
        int od = 0, orq = 0;
        for (int i = 0; i < nd; i++) if (don[i].status == OPEN) od++;
        for (int i = 0; i < nr; i++) if (req[i].status == OPEN) orq++;
        cout << "\nHour " << clockHour << " | profiles " << profiles.size() << " | open donations " << od
             << " | open requests " << orq << " | matches " << history.size() << "\n";
        if (!donHeap.empty()) cout << "Next to expire: donation #" << donHeap.top().id << " at hour " << donHeap.top().key << "\n";
        if (!reqHeap.empty()) cout << "Most urgent: request #" << reqHeap.top().id << " (urgency " << reqHeap.top().key << ")\n";
        history.print("");
    }

    void save() {
        ofstream fp("profiles.txt"), fd("donations.txt"), fr("requests.txt"), fm("matches.txt"), fx("meta.txt");
        profiles.save(fp);
        for (int i = 0; i < nd; i++)
            fd << don[i].id << ' ' << don[i].donor << ' ' << don[i].food << ' ' << don[i].qty << ' ' << don[i].left
               << ' ' << don[i].expiry << ' ' << don[i].status << ' ' << don[i].x << ' ' << don[i].y << '\n';
        for (int i = 0; i < nr; i++)
            fr << req[i].id << ' ' << req[i].recip << ' ' << req[i].food << ' ' << req[i].qty << ' ' << req[i].left
               << ' ' << req[i].urgency << ' ' << req[i].status << ' ' << req[i].x << ' ' << req[i].y << '\n';
        history.save(fm);
        fx << clockHour << ' ' << nextMatch << ' ' << countD << ' ' << countR << '\n';
        cout << "Data saved.\n";
    }

    void load() {
        ifstream fx("meta.txt");
        if (!(fx >> clockHour >> nextMatch >> countD >> countR)) return;
        ifstream fp("profiles.txt"), fd("donations.txt"), fr("requests.txt"), fm("matches.txt");
        Profile p;
        while (fp >> p.id >> p.type >> p.phone >> p.x >> p.y >> p.active) {
            getline(fp, p.name);
            p.name = p.name.substr(1);
            profiles.put(p);
        }
        Donation d;
        while (nd < MAXN && fd >> d.id >> d.donor >> d.food >> d.qty >> d.left >> d.expiry >> d.status >> d.x >> d.y) {
            don[nd++] = d;
            if (d.status == OPEN) { donHeap.push({d.expiry, d.id, d.id}); grid.add(d.id, d.x, d.y); }
        }
        Request r;
        while (nr < MAXN && fr >> r.id >> r.recip >> r.food >> r.qty >> r.left >> r.urgency >> r.status >> r.x >> r.y) {
            req[nr++] = r;
            if (r.status == OPEN) reqHeap.push({r.urgency, r.id, r.id});
        }
        Match m;
        while (fm >> m.id >> m.donor >> m.recip >> m.food >> m.qty >> m.dist >> m.time) history.add(m);
        cout << "Loaded " << profiles.size() << " profiles, " << nd << " donations, " << nr << " requests.\n";
    }

public:
    System() { nd = nr = clockHour = nextMatch = countD = countR = 0; }

    void run() {
        load();
        while (true) {
            cout << "\n##### FOOD RESCUE & DONATION MATCHER #####\n1. Register\n2. Login\n3. Status & matches\n"
                 << "4. Advance clock\n5. Road network (graph)\n6. Save & Exit\n";
            int c = readInt("Choice: ", 1, 6);
            if (c == 1) registerUser();
            else if (c == 2) login();
            else if (c == 3) status();
            else if (c == 4) {
                clockHour += readInt("Hours to advance: ", 1, 100);
                matchAll();
            }
            else if (c == 5) graphMenu();
            else { save(); return; }
        }
    }
};

int main() {
    System *app = new System();
    app->run();
    return 0;
}
