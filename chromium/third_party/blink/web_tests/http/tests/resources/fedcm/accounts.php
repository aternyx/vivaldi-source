<?php
header("Content-Type: application/json; charset=utf-8");

if (isset($_COOKIE["noaccounts"])) {
?>
{ "accounts": [] }
<?php } else { ?>
{
 "accounts": [{
   "id": "1234",
   "given_name": "John",
   "name": "John Doe",
   "email": "john_doe@idp.example",
   "picture": "https://idp.example/profile/123",
   "approved_clients": ["123", "456", "789"]
  }, {
   "id": "5678",
   "given_name": "Aisha",
   "name": "Aisha Ahmad",
   "email": "aisha@idp.example",
   "picture": "https://idp.example/profile/567",
   "approved_clients": []
  }]
}
<?php } ?>
