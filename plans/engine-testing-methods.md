# Phương Pháp Testing & Cải Thiện Chess Engine

## 1. AUTOMATED TESTING FRAMEWORKS

### A. Engine vs Engine Testing
**🎯 Mục đích**: So sánh version cũ vs mới tự động

#### Cutechess-cli Framework
```bash
# Chạy 1000 games giữa 2 versions
cutechess-cli -engine cmd="MyChess_old.exe" -engine cmd="MyChess_new.exe" \
  -each tc=40/60 -rounds 500 -games 2 -pgnout results.pgn
```
##### Yêu cầu engine tương thích với cutechess-cli
- Hỗ trợ giao thức UCI:
  - Implement các lệnh `uci`, `isready`, `ucinewgame`, `position`, `go`, `stop`, `quit`.
  - Trả về thông tin `id name`, `id author` và `readyok`.
- Tương tác qua stdin/stdout, không phụ thuộc GUI khi chạy chế độ test.
- Hỗ trợ các tùy chọn command-line (ví dụ `-threads`, `-hash`, `-perft`, `-bench`) để điều chỉnh thông số tìm kiếm.
- Cung cấp binary (MyChess.exe) ở thư mục dễ truy cập hoặc thêm vào PATH.
- (Tùy chọn) Cung cấp script wrapper nếu cần set môi trường trước khi chạy engine.

```bash
# Ví dụ kiểm tra UCI compatibility
MyChess.exe -uci < /dev/null > uci_output.txt
```

**Advantages:**
- Automated testing 24/7
- Statistical significance
- Parallel game execution
- PGN output để phân tích

#### Fishtest-style Framework
**Concept**: Distributed testing như Stockfish
- Multiple machines chạy games
- Central server thu thập kết quả
- Statistical analysis tự động
- A/B testing cho từng feature

### B. Perft Testing (Debugging)
**🎯 Mục đích**: Verify move generation correctness
```cpp
// Test performance và correctness
uint64_t perft(Board& board, int depth) {
    if (depth == 0) return 1;
    uint64_t nodes = 0;
    auto moves = generateMoves(board);
    for (auto move : moves) {
        board.makeMove(move);
        nodes += perft(board, depth - 1);
        board.unmakeMove(move);
    }
    return nodes;
}
```

**Standard Positions:**
- Starting position: depth 6 = 119,060,324 nodes
- Kiwipete: depth 5 = 193,690,690 nodes
- Position 3: depth 6 = 11,030,083 nodes

## 2. BENCHMARKING & PERFORMANCE TESTING

### A. Tactical Test Suites
**🎯 Mục đích**: Đo khả năng giải puzzles

#### Win At Chess (WAC) - 300 positions
```cpp
struct TacticalTest {
    string fen;
    vector<string> bestMoves;
    int timeLimit; // seconds
    int difficulty; // 1-5
};
```

#### Bratko-Kopec Test - 24 positions
- Các positions từ actual games
- Time limit: 30 seconds/position
- Score dựa trên số positions solved

#### Strategic Test Suite (STS)
- 1500 positions chia theo categories
- Pawn structure, piece placement, etc.
- Multi-choice format

### B. Performance Benchmarks
```cpp
// Nodes per second testing
class PerformanceBench {
    void runStandardBench() {
        vector<string> positions = {
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
            "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
            // ... more test positions
        };
        
        for (auto& fen : positions) {
            auto start = chrono::high_resolution_clock::now();
            int score = search(fen, 8); // depth 8
            auto end = chrono::high_resolution_clock::now();
            
            auto duration = chrono::duration_cast<chrono::milliseconds>(end - start);
            cout << "Position: " << fen << " Time: " << duration.count() << "ms" << endl;
        }
    }
};
```

## 3. STATISTICAL ANALYSIS METHODS

### A. ELO Rating Calculation
```python
# Bayesian ELO calculation
def calculate_elo_difference(wins, losses, draws):
    total_games = wins + losses + draws
    score = (wins + 0.5 * draws) / total_games
    
    if score >= 1.0:
        return float('inf')
    if score <= 0.0:
        return float('-inf')
    
    elo_diff = -400 * math.log10(1/score - 1)
    return elo_diff

# Error margin calculation
def elo_error_margin(total_games, confidence=0.95):
    # 95% confidence interval
    return 800 / math.sqrt(total_games)
```

### B. SPRT (Sequential Probability Ratio Test)
```python
# Early stopping cho testing
class SPRT:
    def __init__(self, elo0=0, elo1=5, alpha=0.05, beta=0.05):
        self.elo0 = elo0  # H0: no improvement
        self.elo1 = elo1  # H1: 5 ELO improvement
        self.alpha = alpha  # Type I error
        self.beta = beta   # Type II error
        
    def should_stop(self, wins, losses, draws):
        llr = self.calculate_llr(wins, losses, draws)
        upper_bound = math.log((1-self.beta)/self.alpha)
        lower_bound = math.log(self.beta/(1-self.alpha))
        
        if llr >= upper_bound:
            return "ACCEPT", f"Improvement confirmed with {1-self.alpha:.1%} confidence"
        elif llr <= lower_bound:
            return "REJECT", f"No improvement with {1-self.beta:.1%} confidence"
        else:
            return "CONTINUE", f"Need more games (LLR: {llr:.2f})"
```

## 4. SPECIALIZED TESTING APPROACHES

### A. Opening Book Testing
```cpp
// Test different opening variations
struct OpeningTest {
    string eco_code;
    vector<string> moves;
    int games_played;
    double score_percentage;
    vector<string> problem_lines;
};

void testOpeningPerformance() {
    OpeningBook book("book.bin");
    for (auto& opening : book.getAllOpenings()) {
        // Play 100 games from each opening
        auto results = playGamesFromOpening(opening, 100);
        analyzeOpeningWeaknesses(opening, results);
    }
}
```

### B. Endgame Testing với Tablebase
```cpp
// Verify endgame play accuracy
class EndgameValidator {
    SyzygyTB tablebase;
    
    bool validateEndgamePlay(const string& fen) {
        auto engine_move = searchBestMove(fen, 10);
        auto perfect_move = tablebase.probe(fen);
        
        return (engine_move == perfect_move) || 
               (tablebase.isEquivalent(engine_move, perfect_move));
    }
};
```

### C. Time Management Testing
```cpp
struct TimeTest {
    int time_control; // seconds
    int increment;
    vector<GameResult> results;
    double avg_time_usage;
    int time_troubles; // games with <10% time left
};

void testTimeManagement() {
    vector<int> time_controls = {60, 180, 300, 600, 1800};
    for (auto tc : time_controls) {
        auto results = playGamesWithTimeControl(tc, 100);
        analyzeTimeUsage(results);
    }
}
```

## 5. REGRESSION TESTING

### A. Continuous Integration Pipeline
```yaml
# GitHub Actions workflow
name: Chess Engine CI
on: [push, pull_request]

jobs:
  perft-test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v2
      - name: Build engine
        run: make
      - name: Run perft tests
        run: ./perft_test.sh
      
  tactical-test:
    runs-on: ubuntu-latest
    steps:
      - name: WAC test
        run: ./run_wac_test.sh
        timeout-minutes: 30
      
  strength-test:
    runs-on: ubuntu-latest
    steps:
      - name: Quick strength test
        run: ./quick_strength_test.sh
        # 50 games vs baseline in 15 minutes
```

### B. Automated Regression Detection
```cpp
class RegressionDetector {
    struct Baseline {
        double wac_score;
        double nps_benchmark;
        double elo_estimate;
        string version_hash;
    };
    
    bool detectRegression(const TestResults& current) {
        auto baseline = loadBaseline();
        
        if (current.wac_score < baseline.wac_score - 5) {
            logWarning("WAC score regression detected!");
            return true;
        }
        
        if (current.nps < baseline.nps * 0.9) {
            logWarning("Performance regression detected!");
            return true;
        }
        
        return false;
    }
};
```

## 6. DEVELOPMENT METHODOLOGIES

### A. Feature Flag Testing
```cpp
class FeatureFlags {
    bool enable_transposition_table = true;
    bool enable_killer_moves = false;  // Testing new feature
    bool enable_history_heuristic = true;
    
    void runABTest() {
        // Version A: without killer moves
        auto results_a = runTests(false);
        
        // Version B: with killer moves  
        auto results_b = runTests(true);
        
        auto improvement = calculateImprovement(results_a, results_b);
        if (improvement > 5.0) { // 5 ELO improvement
            enable_killer_moves = true;
            commitFeature("killer_moves");
        }
    }
};
```

### B. Incremental Development
```cpp
// Step-by-step improvements
enum class ImprovementPhase {
    BASIC_SEARCH,           // Alpha-beta + simple eval
    TRANSPOSITION_TABLE,    // Add TT
    QUIESCENCE_SEARCH,     // Add QS
    MOVE_ORDERING,         // Improve move ordering
    ADVANCED_PRUNING,      // LMR, null move, etc.
    EVALUATION_TUNING      // Parameter optimization
};

class DevelopmentPipeline {
    void executePhase(ImprovementPhase phase) {
        auto baseline = getCurrentELO();
        implementFeature(phase);
        auto new_rating = testNewVersion();
        
        if (new_rating > baseline + 10) { // Minimum 10 ELO gain
            mergeFeature();
            updateBaseline(new_rating);
        } else {
            rollbackFeature();
        }
    }
};
```

## 7. AUTOMATED ANALYSIS TOOLS

### A. Game Analysis Pipeline
```python
class GameAnalyzer:
    def analyze_games(self, pgn_file):
        games = chess.pgn.open(pgn_file)
        analysis = {
            'blunders': [],
            'missed_tactics': [],
            'time_trouble_moves': [],
            'opening_problems': [],
            'endgame_errors': []
        }
        
        for game in games:
            self.analyze_single_game(game, analysis)
        
        return self.generate_report(analysis)
    
    def find_improvement_areas(self, analysis):
        # Identify patterns in losses
        # Find tactical themes engine misses
        # Analyze time management issues
        pass
```

### B. Evaluation Function Tuning
```cpp
class ParameterTuner {
    struct EvalParams {
        int pawn_value = 100;
        int knight_value = 320;
        int bishop_value = 330;
        // ... hundreds of parameters
    };
    
    void tune_parameters() {
        auto current_params = loadParams();
        
        // Gradient descent optimization
        for (int iteration = 0; iteration < 1000; iteration++) {
            auto gradient = calculateGradient(current_params);
            current_params = updateParams(current_params, gradient);
            
            if (iteration % 10 == 0) {
                auto strength = testStrength(current_params);
                logProgress(iteration, strength);
            }
        }
    }
};
```

## 8. PRACTICAL TESTING SETUP

### A. Testing Infrastructure
```bash
#!/bin/bash
# automated_test.sh

# Daily strength test
echo "Running daily strength test..."
cutechess-cli -engine cmd="./MyChess" -engine cmd="./baseline_engine" \
    -each tc=40/10 -rounds 100 -games 2 \
    -pgnout "daily_test_$(date +%Y%m%d).pgn"

# Performance benchmark
echo "Running performance benchmark..."
./MyChess bench > "bench_$(date +%Y%m%d).txt"

# Tactical test
echo "Running WAC test..."
./MyChess wac < wac.epd > "wac_$(date +%Y%m%d).txt"

# Email results
python send_results.py
```

### B. Result Analysis Dashboard
```python
# web_dashboard.py
import streamlit as st
import pandas as pd
import plotly.express as px

def create_dashboard():
    st.title("MyChess Engine Development Dashboard")
    
    # ELO progression over time
    elo_data = load_elo_history()
    fig = px.line(elo_data, x='date', y='elo', title='ELO Progression')
    st.plotly_chart(fig)
    
    # Performance metrics
    perf_data = load_performance_data()
    st.metric("Current ELO", perf_data['current_elo'], 
              delta=perf_data['elo_change'])
    st.metric("Nodes/Second", f"{perf_data['nps']:,}")
    
    # Recent test results
    st.subheader("Recent Test Results")
    test_results = load_recent_tests()
    st.dataframe(test_results)
```

## 9. KẾT LUẬN & WORKFLOW THỰC TẾ

### Quy Trình Testing Hàng Ngày:
1. **Automated CI**: Mỗi commit trigger perft + basic tests
2. **Nightly Testing**: 500-1000 games vs baseline
3. **Weekly Deep Testing**: Tactical suites + endgame tests
4. **Monthly Evaluation**: Full strength assessment vs other engines

### Tools Cần Thiết:
- **Cutechess-cli**: Engine tournament management
- **Syzygy Tablebase**: Perfect endgame verification
- **Test Suites**: WAC, BK, STS cho tactical testing
- **Statistics Tools**: Python/R cho data analysis

### Metrics Tracking:
- **ELO Rating**: Primary strength indicator
- **Tactical Score**: WAC/BK test results
- **Performance**: Nodes per second
- **Regression**: Automated detection of weaknesses

Với framework này, bạn có thể develop engine một cách khoa học mà không cần chơi manual games liên tục! 🚀
