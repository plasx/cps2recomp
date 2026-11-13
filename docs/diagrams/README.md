# Online diagrams

These diagrams use Morphus56K’s website design: Space Grotesk headings, Inter body text,
JetBrains Mono labels, dark green surfaces, phosphor-green connections and cyan annotations.
They are drawn SVGs with outlined lettering, so GitHub displays the intended fonts without
external font requests. They contain no generated photographic artwork or game assets.

| Diagram | Meaning |
| --- | --- |
| [Four machines](mvsc-four-machines.svg) | P1 hosts P2/P3/P4; each person has one computer, controller and fighter owner. Teams are P1/P3 versus P2/P4. Every machine runs the full match and rollback. |
| [Recovery and reset](online-recovery-checkpoint.svg) | Historical checkpoint `4b578bf`: retry departures until acknowledgment, preserve reconnect intent, reset the gauge phase at board boot and preserve it through ordinary transitions. |
| [Upstream integration](online-upstream-integration.svg) | `471f7f7` combines upstream `eea67d1` with the four-player branch, retaining ownership, spectators and first-to agreement. Protocol 10; state format 12. |

The diagrams explain behavior, not completed internet playtests. The current gameplay evidence
remains attached to `471f7f7`; the earlier checkpoint retains its own results. A real four-machine
internet match and rematch with physical controllers remains outstanding.

## Design references

- [Huang, *An Eye Tracking Study into the Effects of Graph Layout*](https://arxiv.org/abs/0810.4431):
  the preliminary study found small-angle crossings could slow path searches. These figures keep
  routes uncrossed and use clear branching or merging points.
- [Kim et al., *Towards Visualization Thumbnail Designs that Entice Reading Data-driven Articles*](https://arxiv.org/abs/2305.17051):
  the study examines how visual elements and concise labels contribute to attraction and understanding.
  These figures use short labels next to the relevant controls, events and connections.

These are design applications, not findings tested on this networking implementation.

Text equivalents and detailed limits remain in [Online play](../ONLINE.md#four-players-on-four-machines)
and [the testing record](../ONLINE_TESTING.md).
