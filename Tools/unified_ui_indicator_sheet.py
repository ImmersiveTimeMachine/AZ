def draw_indicators(d):
    """Draw one 1920x1080 native indicator reference using only the supplied Draw API.

    No imports, GIMP execution, file writes, or changes to the eight-cell quick
    layout. The isolated 104x72 cards below are state samples, not a new layout.
    """
    t = d.t
    d.rect('Indicator sheet / background', 0, 0, 1920, 1080, t['bg'])
    d.text('CHALK', 56, 30, 44, t['text'], t['head'])
    d.text('INDICATORS / SHARED LANGUAGE', 360, 45, 28, t['text'], t['head'])
    d.right(t['name'], 1864, 53, 17, t['muted'], t['body'])
    d.line('Indicator sheet / header rule', 56, 106, 1864, 106,
           t['edge'], 3 if t['rail'] else 1)
    d.text('Shape identifies purpose. A separate mark carries state.',
           58, 124, 18, t['muted'], t['body'])

    # Destination kinds: these exact symbols are shared with map, journal and HUD.
    d.panel('Destination kinds / panel', 56, 164, 874, 292)
    d.text('DESTINATION TYPE', 80, 183, 25, t['text'], t['head'])
    for kind, label, description, x in (
        ('story', 'STORY', 'Double diamond', 221),
        ('side', 'SIDE QUEST', 'Open circle + centre point', 493),
        ('personal', 'MY MARKER', 'Four open corners', 765),
    ):
        d.role(kind, x, 279, 23)
        d.center(label, x, 321, 22, t[kind], t['head'])
        d.center(description, x, 359, 17, t['muted'], t['body'])
    d.text('Readable by silhouette, including without colour.',
           80, 414, 17, t['muted'], t['body'])

    # Optional is a modifier, so it deliberately does not borrow a marker shape
    # or masquerade as a fourth mutually exclusive progress state.
    d.panel('Objective status / panel', 966, 164, 898, 292)
    d.text('OBJECTIVE STATUS', 990, 183, 25, t['text'], t['head'])
    active_x, optional_x, done_x, failed_x = 1090, 1304, 1518, 1732
    d.outline('Active objective / open badge', active_x - 13, 264, 26, 26, t['text'], 2)
    d.ellipse('Active objective / live point', active_x - 3, 274, 6, 6, t['text'])
    d.center('(optional)', optional_x, 260, 21, t['muted'], t['body'])
    d.line('Completed objective / check start', done_x - 13, 277, done_x - 3, 287, t['text'], 3)
    d.line('Completed objective / check finish', done_x - 3, 287, done_x + 16, 263, t['text'], 3)
    d.line('Failed objective / diagonal one', failed_x - 12, 265, failed_x + 12, 289, t['danger'], 3)
    d.line('Failed objective / diagonal two', failed_x - 12, 289, failed_x + 12, 265, t['danger'], 3)
    for label, detail, x, c in (
        ('ACTIVE', 'In progress', active_x, t['text']),
        ('OPTIONAL', 'Additional objective', optional_x, t['muted']),
        ('COMPLETED', 'Check mark', done_x, t['text']),
        ('FAILED', 'Cross + warning colour', failed_x, t['danger']),
    ):
        d.center(label, x, 321, 21, c, t['head'])
        d.center(detail, x, 359, 16, t['muted'], t['body'])
    d.text('An optional objective can also be active, completed or failed.',
           990, 414, 16, t['muted'], t['body'])

    # Current HUD mode versus the inverse destination displayed in quick slot 0.
    d.panel('Modes and health / panel', 56, 490, 874, 464)
    d.text('MODE / CURRENT STATE & NEXT ACTION', 80, 509, 24, t['text'], t['head'])
    d.text('HUD: CURRENT MODE', 102, 556, 15, t['muted'], t['body'])
    d.text('QUICK SLOT 0: DESTINATION', 530, 556, 15, t['muted'], t['body'])
    for current, destination, label, action_label, y in (
        ('explore', 'fight', 'EXPLORE', 'FIGHT', 591),
        ('fight', 'explore', 'FIGHT', 'EXPLORE', 687),
    ):
        d.icon(current, 102, y + 5, 43, 43, t['text'])
        d.text(label, 166, y + 10, 27, t['text'], t['head'])
        d.line('Mode direction / connector', 370, y + 29, 457, y + 29, t['muted'], 1.5)
        d.poly('Mode direction / arrow', [(457, y + 24), (466, y + 29), (457, y + 34)], t['muted'])
        x = 552
        d.rect('Slot 0 / fixed 104x72 fill', x, y - 4, 104, 72, t['panel'])
        d.outline('Slot 0 / stable cell edge', x, y - 4, 104, 72, t['edge'], 1)
        d.right('0', x + 97, y, 15, t['muted'], t['head'])
        d.icon(destination, x + 33, y + 2, 38, 38, t['text'])
        d.center(action_label, x + 52, y + 43, 15, t['text'], t['head'])
        d.text('Press 0: ' + action_label.title(), 687, y + 17, 18, t['text'], t['body'])
    d.text('HUD names the present mode. Slot 0 names where the action takes you.',
           80, 775, 16, t['muted'], t['body'])
    d.line('Health / separator', 80, 811, 906, 811)
    d.text('HEALTH', 80, 827, 19, t['text'], t['head'])
    d.text('NORMAL', 105, 859, 15, t['muted'], t['body'])
    d.right('72 / 100', 415, 854, 20, t['text'], t['head'])
    d.poly('Normal health / heart', [(96, 903), (85, 893), (85, 884), (90, 881),
                                   (96, 885), (102, 881), (107, 884), (107, 893)], t['text'])
    d.rect('Normal health / track', 125, 890, 290, 8, t['edge'], 60)
    d.rect('Normal health / value', 125, 890, 290 * .72, 8, t['text'])
    d.text('CRITICAL', 529, 859, 15, t['danger'], t['body'])
    d.right('16 / 100', 884, 854, 20, t['danger'], t['head'])
    d.poly('Critical health / warning triangle', [(536, 880), (547, 901), (525, 901)], t['danger'])
    d.center('!', 536, 882, 15, t['bg'], t['bold'])
    d.rect('Critical health / track', 560, 890, 324, 8, t['edge'], 60)
    d.rect('Critical health / short value', 560, 890, 324 * .16, 8, t['danger'])
    d.text('Critical health uses a short fill and warning shape as well as colour.',
           80, 923, 15, t['muted'], t['body'])

    # Card content/geometry stays identical. Labels and motion notes stay outside.
    d.panel('Quick-select states / panel', 966, 490, 898, 464)
    d.text('QUICK SELECT / CARD FEEDBACK', 990, 509, 24, t['text'], t['head'])
    d.text('104 × 72 cells. State feedback never moves or resizes a slot.',
           990, 552, 17, t['muted'], t['body'])
    for state, x in (('FOCUSED', 1068), ('EQUIPPED', 1368), ('ASSIGNING', 1668)):
        y = 629
        d.center(state, x + 52, 591, 21, t['text'], t['head'])
        d.rect(state + ' card / unchanged content fill', x, y, 104, 72, t['panel'])
        if state == 'ASSIGNING':
            for k, points in enumerate(((x, y, x + 104, y), (x + 104, y, x + 104, y + 72),
                                         (x + 104, y + 72, x, y + 72), (x, y + 72, x, y))):
                d.line('Assigning / border pulse bright phase ' + str(k), *points, t['text'], 2.5, 100)
        else:
            d.outline(state + ' card / frame', x, y, 104, 72,
                      t['text'] if state == 'FOCUSED' else t['edge'], 2 if state == 'FOCUSED' else 1)
        d.text('M16', x + 8, y + 4, 14, t['text'], t['head'])
        d.right('1', x + 97, y + 4, 15, t['muted'], t['head'])
        d.icon('rifle', x + 8, y + 14, 86, 43, t['text'])
        d.right('17 / 30', x + 97, y + 44, 16, t['text'], t['head'])
        if state == 'EQUIPPED':
            d.rect('Equipped / persistent bottom notch', x + 44, y + 69, 16, 3, t['text'])
    for x, first, second in (
        (1120, 'Steady bright frame', 'Navigation focus'),
        (1420, 'Steady lower notch', 'Currently equipped'),
        (1720, 'Border pulse only', 'Awaiting assignment'),
    ):
        d.center(first, x, 739, 18, t['text'], t['body'])
        d.center(second, x, 774, 16, t['muted'], t['body'])
    d.line('Quick state notes / separator', 990, 823, 1840, 823)
    d.text('ASSIGNING: animate only the border. Icon, text and fill stay still.',
           990, 845, 17, t['text'], t['body'])
    d.text('Focus and equipment state can coexist; their signs remain separate.',
           990, 890, 16, t['muted'], t['body'])

    d.line('Indicator sheet / footer rule', 56, 995, 1864, 995)
    d.text('REFERENCE SHEET  /  Theme comparison only  /  Example values',
           58, 1019, 17, t['muted'], t['body'])
    d.right('No change to the eight-cell layout', 1864, 1019, 17, t['muted'], t['body'])
