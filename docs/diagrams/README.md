# Four-machine illustration

[View the figure](../screenshots/mvsc-four-machines.jpg).

The figure explains four separate computers, one controller and one fighter owner per person.
P1 hosts; P2, P3 and P4 each exchange inputs with the host. P1/P3 form one team, P2/P4 the other.
Every computer runs the complete game and rollback. The monitors are schematic illustrations,
not game screenshots. The figure is not evidence of a completed four-machine internet playtest.

## Design references

These papers informed the presentation choices; none evaluated this implementation or establishes
one universally best visual style.

- [Huang, *An Eye Tracking Study into the Effects of Graph Layout*](https://arxiv.org/abs/0810.4431).
  This preliminary study found small-angle crossings could slow path searches. The figure uses three
  separate, uncrossed host-to-guest links.
- [Kim et al., *Towards Visualization Thumbnail Designs that Entice Reading Data-driven Articles*](https://arxiv.org/abs/2305.17051).
  The study examines how recognizable imagery, highlights and labels contribute to attraction and
  understanding. Here, recognizable computers and arcade controllers accompany short, adjacent labels.
- [Xiao et al., *Let the Chart Spark*](https://arxiv.org/abs/2304.14630).
  This work explores generated imagery combined with chart structure. Here, the illustration was checked
  separately for machine/controller counts, team ownership, input paths and readable labels.

## Artwork provenance

Original explanatory artwork generated with OpenAI's image-generation tool and reviewed against the
network implementation. No game art, gameplay capture or photographed test setup was used. The JPEG
is a compact export of the generated artwork. [Generation brief](mvsc-four-machines.prompt.txt).

Text equivalents, setup instructions and the distinction between local tests and outstanding internet
tests remain in [Online play](../ONLINE.md#four-players-on-four-machines) and
[the testing record](../ONLINE_TESTING.md).
