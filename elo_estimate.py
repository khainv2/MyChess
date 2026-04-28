#!/usr/bin/env python3
"""
Compute ELO estimate from match results.

  python3 elo_estimate.py --wins W --draws D --losses L --opponent-elo X

Formula:
  score = (W + 0.5*D) / N
  delta_elo = -400 * log10(1/score - 1)        # standard ELO inverse
  std_err   = 400 / log(10) * sqrt(score*(1-score)/N) / (score*(1-score))
            ≈ 400 / sqrt(N) * 1/(2*sqrt(score*(1-score)))   # near 0.5

Reports ΔELO, estimated MyChess ELO, and a 95% CI (±1.96·SE).
"""
import argparse, math, sys

def parse():
    p = argparse.ArgumentParser()
    p.add_argument('--wins',   type=int, required=True)
    p.add_argument('--draws',  type=int, required=True)
    p.add_argument('--losses', type=int, required=True)
    p.add_argument('--opponent-elo', type=int, required=True,
                   help='UCI_Elo (or estimated rating) of the opponent')
    return p.parse_args()

def delta_elo(score):
    if score <= 0:    return -800.0   # cap
    if score >= 1:    return  800.0
    return -400.0 * math.log10(1.0/score - 1.0)

def main():
    a = parse()
    n = a.wins + a.draws + a.losses
    if n == 0:
        print("no games"); sys.exit(1)

    score = (a.wins + 0.5*a.draws) / n
    de = delta_elo(score)
    elo = a.opponent_elo + de

    # Standard error of ELO estimate via Wald approximation around p=score.
    # Var(score) = score*(1-score)/N. dELO/dscore = 400/(ln10 * score*(1-score))
    if 0 < score < 1:
        var_s = score * (1 - score) / n
        d_de_dp = 400.0 / (math.log(10) * score * (1 - score))
        se = math.sqrt(var_s) * d_de_dp
    else:
        se = float('inf')

    ci95 = 1.96 * se

    print(f"  Games:         {n}  (W {a.wins} / D {a.draws} / L {a.losses})")
    print(f"  Score:         {score*100:.1f}%")
    print(f"  Opponent ELO:  {a.opponent_elo}")
    print(f"  Δ ELO:         {de:+.1f}")
    print(f"  Estimated ELO: {elo:.0f}")
    print(f"  95% CI:        ±{ci95:.0f}  →  [{elo-ci95:.0f}, {elo+ci95:.0f}]")

if __name__ == '__main__':
    main()
