# CLAS Training Data (Reserved)

This folder is reserved for future training logs. Alpha builds must not write to this directory.

Planned formats:
- JSONL for event streams
- Parquet for batch analytics

Retention policy (planned):
- Short-term raw logs (days)
- Long-term aggregated summaries (weeks/months)

Activation rule:
- Training logging will only be enabled after explicit user consent and a stable core alpha.
