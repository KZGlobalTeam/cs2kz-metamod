"""Exercise the production record-cache SQL on a disposable in-memory database.
Run from the repository root: python tests/progress/record_cache_test.py
"""
from pathlib import Path
import re
import sqlite3
import unittest

SOURCE = Path(__file__).resolve().parents[2] / 'src/kz/db/queries/course_top.h'
QUERIES = dict(re.findall(r'constexpr char (sql_getsrs(?:pro)?)\[\] = R"\((.*?)\)";', SOURCE.read_text(encoding='utf-8'), re.S))

class RecordCacheTests(unittest.TestCase):
    def setUp(self):
        self.db = sqlite3.connect(':memory:')
        self.db.executescript('''
            CREATE TABLE Maps (ID INTEGER, Name TEXT);
            CREATE TABLE MapCourses (ID INTEGER, MapID INTEGER);
            CREATE TABLE Times (ID TEXT, SteamID64 INTEGER, MapCourseID INTEGER,
                ModeID INTEGER, StyleIDFlags INTEGER, RunTime REAL, Teleports INTEGER, Metadata TEXT);
            CREATE TABLE Bans (ID INTEGER, SteamID64 INTEGER, ExpiresAt TEXT);
            INSERT INTO Maps VALUES (1, 'kz_grotto'), (2, 'other_map');
            INSERT INTO MapCourses VALUES (2, 1), (3, 1), (4, 2);
        ''')

    def tearDown(self):
        self.db.close()

    def run_entry(self, uuid, time, *, player=1, course=2, mode=0, styles=0, teleports=0):
        self.db.execute('INSERT INTO Times VALUES (?, ?, ?, ?, ?, ?, ?, ?)',
                        (uuid, player, course, mode, styles, time, teleports, '{"fixture":"'+uuid+'"}'))

    def records(self, pro=True):
        # Execute the strings used by QueryAllRecords, not a rewritten approximation.
        rows=self.db.execute(QUERIES['sql_getsrspro' if pro else 'sql_getsrs'] % 'kz_grotto').fetchall()
        return {(row[1],row[2]):row for row in rows}

    def test_uuid_and_metadata_belong_to_same_winner(self):
        self.run_entry('slow', 50)
        self.run_entry('fast', 40)
        row=self.records()[(2,0)]
        self.assertEqual((row[0],row[3],row[4]), (40,'{"fixture":"fast"}','fast'))

    def test_pro_excludes_teleports(self):
        self.run_entry('tp', 10, teleports=1)
        self.run_entry('pro', 40)
        self.assertEqual(self.records()[(2,0)][4], 'pro')
        self.assertEqual(self.records(False)[(2,0)][4], 'tp')

    def test_styles_cannot_displace_unstyled_record(self):
        self.run_entry('style', 10, styles=1)
        self.run_entry('normal', 40)
        for pro in (False, True): self.assertEqual(self.records(pro)[(2,0)][4], 'normal')

    def test_bans_and_expired_bans(self):
        self.run_entry('banned', 10, player=2)
        self.run_entry('expired', 20, player=3)
        self.run_entry('normal', 40)
        self.db.executescript("INSERT INTO Bans VALUES (1,2,NULL),(2,3,'2000-01-01');")
        for pro in (False, True): self.assertEqual(self.records(pro)[(2,0)][4], 'expired')

    def test_equal_times_have_one_deterministic_uuid(self):
        self.run_entry('b', 40)
        self.run_entry('a', 40)
        rows=self.records()
        self.assertEqual(len(rows),1)
        self.assertEqual(rows[(2,0)][4], 'a')

    def test_map_course_and_mode_are_independent(self):
        self.run_entry('garden',40)
        self.run_entry('other-course',30,course=3)
        self.run_entry('other-mode',20,mode=1)
        self.run_entry('other-map',1,course=4)
        self.assertEqual({k:v[4] for k,v in self.records().items()},
                         {(2,0):'garden',(3,0):'other-course',(2,1):'other-mode'})

    def test_refresh_observes_new_and_removed_records(self):
        self.run_entry('old',40)
        self.assertEqual(self.records()[(2,0)][4],'old')
        self.run_entry('new',30)
        self.assertEqual(self.records()[(2,0)][4],'new')
        self.db.execute("DELETE FROM Times WHERE ID='new'")
        self.assertEqual(self.records()[(2,0)][4],'old')
        self.db.execute('DELETE FROM Times')
        self.assertEqual(self.records(),{})

if __name__=='__main__': unittest.main(verbosity=2)
