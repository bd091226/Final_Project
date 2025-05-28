import unittest
import requests


class TestAPI(unittest.TestCase):
    base_url = 'http://localhost:8080/api'

    def test_get(self, endpoint):
        response = requests.get(self.base_url + endpoint)

        # TC1. 응답값이 200 status인 지 확인
        self.assertEqual(response.status_code, 200, "Status code shoud be 200.")

        # TC2. 응답값이 JSON 형식인 지 확인
        try:
            data = response.json()
        except ValueError:
            self.fail("Response was not in JSON format.")

        # TC3. 사용자 목록이 비어 있지 않은 지 확인
        self.assertTrue(len(data) > 0, "List is empty.")
        
    def test_get_id(self, endpoint, id):
        response = requests.get(self.base_url + endpoint + "/" + id)

        # TC1. 응답값이 200 status인 지 확인
        self.assertEqual(response.status_code, 200, "Status code shoud be 200.")

        # TC2. 응답값이 JSON 형식인 지 확인
        try:
            data = response.json()
        except ValueError:
            self.fail("Response was not in JSON format.")

        # TC3. 사용자 목록이 비어있지 않은 지 확인
        self.assertTrue(len(data) > 0, "User list is empty.")

class TestTodosAPI(unittest.TestCase):
    base_url = 'https://koreanjson.com'

    def test_get_todos(self):
        endpoint = '/todos'
        response = requests.get(self.base_url + endpoint)

        # TC1. 응답값이 200 status인 지 확인
        self.assertEqual(response.status_code, 200, "Status code shoud be 200.")

        # TC2. 응답값이 JSON 형식인 지 확인
        try:
            data = response.json()
        except ValueError:
            self.fail("Response was not in JSON format.")

        # TC3. 사용자 목록이 비어있지 않은 지 확인
        self.assertTrue(len(data) > 0, "Todos list is empty.")

    def test_get_todos_id(self):
        endpoint = '/todos/1'
        response = requests.get(self.base_url + endpoint)

        # TC1. 응답값이 200 status인 지 확인
        self.assertEqual(response.status_code, 200, "Status code shoud be 200.")

        # TC2. 응답값이 JSON 형식인 지 확인
        try:
            data = response.json()
        except ValueError:
            self.fail("Response was not in JSON format.")

        # TC3. 사용자 목록이 비어있지 않은 지 확인
        self.assertTrue(len(data) > 0, "Todos list is empty.")


if __name__ == "__main__":
    unittest.main()
