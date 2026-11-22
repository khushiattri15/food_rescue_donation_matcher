# Food Rescue & Donation Matcher

Data Structures Lab (15B17CI371), Jaypee Institute of Information Technology, Noida
B.Tech CSE, 2nd Year, 3rd Semester. Faculty: Prof Jyoti Singh.

**Team:** Khushi Attri (992501030120), Shreya Sharma (992501030130), Mayank Sethi (992501030144)

A menu-driven system that matches surplus food from donors (restaurants, caterers, households) with nearby
recipients (shelters, NGOs) before it spoils.

## Data structures and algorithms used

| Concept | Where it is used |
|---|---|
| Structs / classes | Donor, recipient, donation, request and match records |
| Min-heap (priority queue) | Donations ordered by expiry, requests by urgency |
| Grid spatial index | Finds donations near a request without checking all of them |
| Queue (linked list) | Intake pipeline for donations and requests; also used by BFS |
| Hash map | Profile lookup by ID |
| Linked list | Match history |
| Graph (adjacency matrix and list) | Road network of 8 collection hubs |
| BFS / DFS | Hub traversal, fewest-hop delivery route |
| Prim's and Kruskal's MST | Cheapest road network connecting all hubs |
| File handling | Saves profiles, donations, requests and matches between runs |

## Run the C++ program

```
g++ -std=c++11 -o foodrescue food_rescue_matcher.cpp
./foodrescue
```

## Web prototype

Open `index.html` in a browser. It runs the same logic in JavaScript, with a live map, heaps, match history and a graph lab.
